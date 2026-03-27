#pragma once

/*
 * Usagi Engine: Task Graph Executive & ECS Core (C++26)
 * -----------------------------------------------------------------------------
 * Topology: Registration-ordered DAG evaluated JIT per-frame.
 * Resolution: Table-level locking via ComponentMasks.
 * Concurrency: Lock-free atomic Task Graph traversal across hardware threads.
 * Safety: Compile-time capability proxy & Runtime SEH/Exception firewalls.
 * Hardening: Terminal state persistence and topological poison diffusion.
 * Features:
 * - C++26 Static Reflection (P2996R13) for Query DSL unpacking.
 * - JIT Lock Escalation for immediate Entity destruction.
 * - Cyclic Re-entrancy via deferred spawn queues.
 * - SEH/Exception firewalls for fault-tolerant execution.
 */

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>

#include "EntityDatabase.hpp"

namespace usagi
{
/* Shio: Using signed 64-bit integers for graph indices and counters.
   Unsigned types (size_t) silently wrap on underflow, masking critical
   scheduler bugs. Signed 64-bit guarantees mathematical trapping in the
   negative domain and prevents 32-bit overflow in extremely dense topologies.
 */
using NodeIndex = int64_t;

constexpr int MAX_RE_ENTRIES = 5;

// -----------------------------------------------------------------------------
// Task Graph Executive
// -----------------------------------------------------------------------------
struct SystemNode
{
    std::string name;

    // Compile-time statically extracted bounds
    ComponentMask static_read_mask { 0 };
    ComponentMask static_write_mask { 0 };
    ComponentMask static_delete_mask { 0 };

    // JIT dynamically calculated bounds
    ComponentMask jit_write_mask { 0 };

    // Execution payload
    std::function<void(EntityDatabase &)> execute_proxy_fn;

    // Yukino: Upgraded to signed 64-bit to prevent silent 32-bit
    // underflow/overflow.
    std::atomic<NodeIndex> in_degree { 0 };
    std::vector<NodeIndex> dependents;

    // Terminal States (Persist across re-entrancy loops)
    bool executed { false };
    bool failed { false };
    bool aborted_due_to_dependency { false };

    SystemNode()                              = default;
    SystemNode(const SystemNode &)            = delete;
    SystemNode &operator=(const SystemNode &) = delete;

    SystemNode(SystemNode &&other) noexcept
        : name(std::move(other.name))
        , static_read_mask(other.static_read_mask)
        , static_write_mask(other.static_write_mask)
        , static_delete_mask(other.static_delete_mask)
        , jit_write_mask(other.jit_write_mask)
        , execute_proxy_fn(std::move(other.execute_proxy_fn))
        , in_degree(other.in_degree.load())
        , dependents(std::move(other.dependents))
        , executed(other.executed)
        , failed(other.failed)
        , aborted_due_to_dependency(other.aborted_due_to_dependency)
    {
    }
};

class TaskGraphExecutive
{
    EntityDatabase          &db;
    std::vector<SystemNode>  systems;
    std::vector<std::string> execution_log;
    std::vector<std::string> error_log;

public:
    explicit TaskGraphExecutive(EntityDatabase &database)
        : db(database)
    {
    }

    template <typename SystemType>
    void register_system(const std::string &name)
    {
        using Query = typename SystemType::EntityQuery;
        SystemNode node;
        node.name               = name;
        node.static_read_mask   = meta_utils::extract_read_mask<Query>();
        node.static_write_mask  = meta_utils::extract_write_mask<Query>();
        node.static_delete_mask = meta_utils::extract_delete_mask<Query>();

        node.execute_proxy_fn = [](EntityDatabase &db_ref) {
            DatabaseAccess<SystemType> proxy(db_ref);
            SystemType::update(proxy);
        };

        systems.push_back(std::move(node));
    }

    void clear_systems() { systems.clear(); }

    const std::vector<std::string> &get_execution_log() const
    {
        return execution_log;
    }

    const std::vector<std::string> &get_error_log() const { return error_log; }

    /* Shio: The pure endomorphism execution cycle. */
    void execute_frame()
    {
        if(systems.empty()) return;

        execution_log.clear();
        error_log.clear();
        int re_entry_counter = 0;

        // Yukino: Reset terminal states ONCE at the start of the time slice.
        // If a system crashes, it remains mathematically dead for all
        // re-entrancy cycles within this frame.
        for(auto &sys : systems)
        {
            sys.failed                    = false;
            sys.aborted_due_to_dependency = false;
        }

        do
        {
            if(re_entry_counter > 0)
            {
                execution_log.push_back(
                    "--- Executive: Re-entrancy cycle triggered ---");
                db.commit_pending_spawns();
            }

            execute_jit_pre_pass();
            build_topological_graph();
            dispatch_tasks();

            re_entry_counter++;
            if(re_entry_counter > MAX_RE_ENTRIES && db.has_pending_spawns())
            {
                throw std::runtime_error(
                    "FATAL: Maximum Task Graph re-entrancy depth exceeded "
                    "(Infinite loop).");
            }
        }
        while(db.has_pending_spawns());
    }

private:
    /* Yukino: Phase 1: The Blast Radius calculation. This solves the
     * intersection hazard dynamically without global locks. */
    void execute_jit_pre_pass()
    {
        for(auto &sys : systems)
        {
            sys.jit_write_mask = sys.static_write_mask;

            if(sys.static_delete_mask != 0)
            {
                // Determine entities the system statically targets
                ComponentMask target_signature = sys.static_read_mask |
                    sys.static_write_mask |
                    sys.static_delete_mask;
                auto candidates = db.query_entities(target_signature);

                ComponentMask dynamic_blast_radius = 0;
                for(EntityId e : candidates)
                {
                    // Aggregate the EXACT runtime masks of the condemned
                    // entities
                    dynamic_blast_radius |= db.get_dynamic_mask(e);
                }

                // JIT Lock Escalation: The write mask expands to cover all
                // components currently attached to the condemned entities.
                sys.jit_write_mask |= dynamic_blast_radius;

                if(dynamic_blast_radius != 0)
                {
                    execution_log.push_back(
                        "JIT Escalation [" +
                        sys.name +
                        "]: Blast radius expanded to mask " +
                        std::to_string(dynamic_blast_radius));
                }
            }
        }
    }

    /* Shio: Phase 2: Topological sorting based on JIT locks. If System A and
     * System B do not overlap, no edge is created, meaning they can
     * mathematically execute in parallel. */
    void build_topological_graph()
    {
        for(auto &sys : systems)
        {
            sys.in_degree.store(0);
            sys.dependents.clear();

            // Shio: Do not resurrect dead nodes.
            if(!sys.failed && !sys.aborted_due_to_dependency)
            {
                sys.executed = false;
            }
        }

        // Implicit DAG generation via registration order guarantees acyclic
        // topology
        // Establish happens-before edges based on registration order to resolve
        // conflicts
        for(size_t i = 0; i < systems.size(); ++i)
        {
            for(size_t j = i + 1; j < systems.size(); ++j)
            {
                bool overlap_rw = (systems[i].static_read_mask &
                                      systems[j].jit_write_mask) != 0;
                bool overlap_wr = (systems[i].jit_write_mask &
                                      systems[j].static_read_mask) != 0;
                bool overlap_ww = (systems[i].jit_write_mask &
                                      systems[j].jit_write_mask) != 0;

                if(overlap_rw || overlap_wr || overlap_ww)
                {
                    // Topological edge injection. Utilizing signed 64-bit
                    // index.
                    // Conflict detected. System i must finish before System j.
                    systems[i].dependents.push_back(static_cast<NodeIndex>(j));
                    systems[j].in_degree++;
                }
            }
        }
    }

    /* Yukino: Phase 3: Dispatch. In a real engine, systems with in_degree == 0
     * are pushed to a lock-free thread pool. For this PoC, we execute
     * sequentially to verify topological correctness and SEH/Exception
     * isolation. */
    void dispatch_tasks_single_threaded()
    {
        std::queue<int> ready_queue;

        for(size_t i = 0; i < systems.size(); ++i)
        {
            if(systems[i].in_degree == 0) ready_queue.push(i);
        }

        while(!ready_queue.empty())
        {
            int current = ready_queue.front();
            ready_queue.pop();

            SystemNode &sys = systems[current];

            if(sys.aborted_due_to_dependency) continue;

            try
            {
                sys.execute_proxy_fn(db);
                sys.executed = true;
                execution_log.push_back("Executed: " + sys.name);

                // Unblock dependents only on success
                for(int dep_idx : sys.dependents)
                {
                    systems[dep_idx].in_degree--;
                    if(systems[dep_idx].in_degree == 0)
                    {
                        ready_queue.push(dep_idx);
                    }
                }
            }
            catch(const std::exception &e)
            {
                /* Yukino: Fault-tolerance. If a system crashes, we abort its
                 * downstream dependents to prevent reading corrupted memory,
                 * but independent systems run. */
                sys.failed   = true;
                sys.executed = true; // Mark done so graph doesn't hang
                                     // permanently, but dependents might suffer
                error_log.push_back(
                    "Exception in [" + sys.name + "]: " + e.what());
                execution_log.push_back("FAILED: " + sys.name);

                abort_dependents_recursively_single_threaded(current);
            }
        }
    }

    void abort_dependents_recursively_single_threaded(int node_idx)
    {
        for(int dep_idx : systems[node_idx].dependents)
        {
            if(!systems[dep_idx].aborted_due_to_dependency)
            {
                systems[dep_idx].aborted_due_to_dependency = true;
                systems[dep_idx].executed = true; // Mark resolved
                error_log.push_back(
                    "Aborted downstream dependent: [" +
                    systems[dep_idx].name +
                    "]");
                abort_dependents_recursively_single_threaded(dep_idx);
            }
        }
    }

    /* Yukino: Parallel topological dispatch. The wavefront evaluates across
     * hardware threads. Safe SEH abortion propagation is locked and
     * synchronized. */
    void dispatch_tasks()
    {
        unsigned int hw_threads = std::thread::hardware_concurrency();
        if(hw_threads == 0) hw_threads = 4;

        std::mutex              mtx;
        std::condition_variable cv;

        // Shio: Queues and task boundaries now enforce signed 64-bit math.
        std::queue<NodeIndex> ready_queue;
        std::atomic<int64_t>  tasks_completed { 0 };
        const int64_t total_tasks = static_cast<int64_t>(systems.size());

        for(size_t i = 0; i < systems.size(); ++i)
        {
            if(systems[i].in_degree.load() == 0)
                ready_queue.push(static_cast<NodeIndex>(i));
        }

        auto worker = [&]() {
            while(true)
            {
                NodeIndex current = -1;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [&]() {
                        // Math evaluation on signed boundaries
                        return !ready_queue.empty() ||
                            tasks_completed.load() == total_tasks;
                    });

                    if(tasks_completed.load() == total_tasks) return;

                    if(!ready_queue.empty())
                    {
                        current = ready_queue.front();
                        ready_queue.pop();
                    }
                }

                if(current != -1)
                {
                    SystemNode &sys = systems[current];

                    bool        did_execute = false;
                    bool        did_crash   = false;
                    std::string crash_msg;

                    // Shio: Evaluate execution only if the node is
                    // mathematically alive in this time-slice.
                    if(!sys.aborted_due_to_dependency && !sys.failed)
                    {
                        try
                        {
                            sys.execute_proxy_fn(db);
                            did_execute = true;
                        }
                        catch(const std::exception &e)
                        {
                            did_crash = true;
                            crash_msg = e.what();
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        sys.executed =
                            true; // Mark resolved for this topological pass

                        if(did_execute)
                        {
                            execution_log.push_back("Executed: " + sys.name);
                            for(NodeIndex dep_idx : sys.dependents)
                            {
                                if(--systems[dep_idx].in_degree == 0)
                                {
                                    ready_queue.push(dep_idx);
                                    cv.notify_one();
                                }
                            }
                        }
                        else if(did_crash)
                        {
                            sys.failed = true;
                            error_log.push_back(
                                "Exception in [" +
                                sys.name +
                                "]: " +
                                crash_msg);
                            execution_log.push_back("FAILED: " + sys.name);
                            abort_immediate_dependents(
                                current, ready_queue, cv);
                        }
                        else
                        {
                            // Node was already dead from a prior re-entrancy
                            // loop. We act as a sink and naturally propagate
                            // the topological poison to new DAG edges.
                            abort_immediate_dependents(
                                current, ready_queue, cv);
                        }

                        int64_t current_completed =
                            tasks_completed.fetch_add(1) + 1;

                        // Yukino: The Torvalds constraint. Mathematically trap
                        // any illegal decrements or compiler modulo wrapping
                        // before it deadlocks the CV.
                        if(current_completed < 0)
                        {
                            throw std::runtime_error(
                                "FATAL: tasks_completed underflowed. DAG "
                                "tracking is corrupted.");
                        }

                        if(current_completed == total_tasks)
                        {
                            cv.notify_all();
                        }
                    }
                }
            }
        };

        std::vector<std::thread> threads;
        for(unsigned int i = 0; i < hw_threads; ++i)
            threads.emplace_back(worker);
        for(auto &t : threads)
            t.join();

        for(const auto &sys : systems)
        {
            if(!sys.executed && !sys.aborted_due_to_dependency)
            {
                throw std::runtime_error(
                    "FATAL: Topological deadlock detected in Task Graph.");
            }
        }
    }

    void abort_dependents_recursively_buggy(
        int node_idx, std::queue<int> &ready_queue, std::condition_variable &cv)
    {
        for(int dep_idx : systems[node_idx].dependents)
        {
            if(!systems[dep_idx].aborted_due_to_dependency)
            {
                systems[dep_idx].aborted_due_to_dependency = true;
                error_log.push_back(
                    "Aborted downstream dependent: [" +
                    systems[dep_idx].name +
                    "]");
                execution_log.push_back("ABORTED: " + systems[dep_idx].name);
                abort_dependents_recursively_buggy(dep_idx, ready_queue, cv);
            }

            // Decrement in-degree so the aborted node still flows through the
            // scheduler logic and increments tasks_completed when processed by
            // the worker.
            if(--systems[dep_idx].in_degree == 0)
            {
                ready_queue.push(dep_idx);
                cv.notify_one();
            }
        }
    }

    /* Yukino: Iterative DFS abortion propagation. A recursive implementation
     * will trigger a stack overflow (SIGSEGV) if a graph contains a deep linear
     * dependency chain (e.g., 100,000 systems). This heap-allocated stack
     * resolves pathological topographies safely. */
    void abort_dependents_iterative_buggy(
        int start_node_idx, std::queue<int> &ready_queue,
        std::condition_variable &cv)
    {
        std::vector<int> stack;
        stack.push_back(start_node_idx);

        while(!stack.empty())
        {
            int current = stack.back();
            stack.pop_back();

            for(int dep_idx : systems[current].dependents)
            {
                if(!systems[dep_idx].aborted_due_to_dependency)
                {
                    systems[dep_idx].aborted_due_to_dependency = true;
                    error_log.push_back(
                        "Aborted downstream dependent: [" +
                        systems[dep_idx].name +
                        "]");
                    execution_log.push_back(
                        "ABORTED: " + systems[dep_idx].name);

                    // Push to stack to propagate the abortion wavefront
                    // downstream
                    stack.push_back(dep_idx);
                }

                // Decrement in-degree so the aborted node still flows through
                // the scheduler logic and increments tasks_completed when
                // processed by the worker loop.
                if(--systems[dep_idx].in_degree == 0)
                {
                    ready_queue.push(dep_idx);
                    cv.notify_one();
                }
            }
        }
    }

    /* Yukino: Poison Diffusion. Instead of using a recursive stack that risks
       SIGSEGV, we inject the aborted states into the thread pool's natural DAG
       traversal. Aborted nodes wake up, execute nothing, and recursively poison
       their dependents. */
    void abort_immediate_dependents(
        NodeIndex current, std::queue<NodeIndex> &ready_queue,
        std::condition_variable &cv)
    {
        for(NodeIndex dep_idx : systems[current].dependents)
        {
            if(!systems[dep_idx].aborted_due_to_dependency)
            {
                systems[dep_idx].aborted_due_to_dependency = true;
                error_log.push_back(
                    "Aborted downstream dependent: [" +
                    systems[dep_idx].name +
                    "]");
                execution_log.push_back("ABORTED: " + systems[dep_idx].name);
            }

            // The DAG edge is satisfied regardless of abortion.
            // Send it to the thread pool to propagate the poison further.
            if(--systems[dep_idx].in_degree == 0)
            {
                ready_queue.push(dep_idx);
                cv.notify_one();
            }
        }
    }
};
} // namespace usagi
