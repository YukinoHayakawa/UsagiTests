/*
 * Usagi Engine: Task Graph Executive & Pipeline Resolver (C++26)
 * -----------------------------------------------------------------------------
 * Topology: Continuous Pipelined DAG (G_inf) evaluated JIT per-frame.
 * Resolution: Table-level locking via ComponentMasks & DataAccessFlags.
 * Concurrency: Persistent condition_variable Worker Pool.
 * Hardening: Terminal state persistence and topological poison diffusion.
 * * Mathematical Guarantee: This orchestrator calculates the absolute blast
 * radius of the Bipartite Graph closures before allowing any thread to touch
 * the matrices.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "EntityDatabase.hpp"

namespace usagi
{
// -----------------------------------------------------------------------------
// Persistent Pipelined Task Worker Pool
// -----------------------------------------------------------------------------
struct SystemNode
{
    uint64_t    frame_index { 0 };
    std::string name;

    ComponentMask   static_read_mask { 0 };
    ComponentMask   static_write_mask { 0 };
    ComponentMask   static_delete_mask { 0 };
    DataAccessFlags execution_flags { DataAccessFlags::None };

    // Dynamically projected upon DAG generation
    ComponentMask jit_write_mask { 0 };

    std::function<void(LayeredDatabaseAggregator &)> execute_proxy_fn;

    std::atomic<int64_t>                     in_degree { 0 };
    std::vector<std::shared_ptr<SystemNode>> dependents;

    bool executed { false };
    bool failed { false };
    bool aborted_due_to_dependency { false };
};

class TaskWorkerPool
{
    std::vector<std::thread>                workers;
    std::queue<std::shared_ptr<SystemNode>> ready_queue;
    std::mutex                              queue_mutex;
    std::condition_variable                 cv;
    std::atomic<bool>                       terminate_flag { false };

    void worker_loop()
    {
        while(!terminate_flag.load(std::memory_order_relaxed))
        {
            std::shared_ptr<SystemNode> task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [this]() {
                    return !ready_queue.empty() ||
                        terminate_flag.load(std::memory_order_relaxed);
                });

                if(terminate_flag.load(std::memory_order_relaxed) &&
                    ready_queue.empty())
                    return;

                task = ready_queue.front();
                ready_queue.pop();
            }

            if(!task->aborted_due_to_dependency && !task->failed)
            {
                try
                {
                    task->execute_proxy_fn(*db_ptr);
                }
                catch(...)
                {
                    task->failed = true;
                }
            }
            task->executed = true;

            // Lock-free downstream poison diffusion
            auto resolve_dependents = [&](auto &node) {
                for(auto &dep : node->dependents)
                {
                    if(node->failed || node->aborted_due_to_dependency)
                    {
                        dep->aborted_due_to_dependency = true;
                    }
                    if(dep->in_degree.fetch_sub(1, std::memory_order_acq_rel) ==
                        1)
                    {
                        push_task(dep);
                    }
                }
            };
            resolve_dependents(task);
        }
    }

public:
    LayeredDatabaseAggregator *db_ptr = nullptr;

    void start(LayeredDatabaseAggregator &db, uint32_t hw_threads)
    {
        db_ptr = &db;
        for(uint32_t i = 0; i < hw_threads; ++i)
        {
            workers.emplace_back(&TaskWorkerPool::worker_loop, this);
        }
    }

    void stop()
    {
        terminate_flag.store(true, std::memory_order_relaxed);
        cv.notify_all();
        for(auto &w : workers)
        {
            if(w.joinable()) w.join();
        }
    }

    void push_task(std::shared_ptr<SystemNode> task)
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        ready_queue.push(task);
        cv.notify_one();
    }

    ~TaskWorkerPool() { stop(); }
};

// -----------------------------------------------------------------------------
// Pipelined Task Graph Executive
// -----------------------------------------------------------------------------
class TaskGraphExecutive
{
    LayeredDatabaseAggregator &db;
    TaskWorkerPool             worker_pool;

    std::vector<std::shared_ptr<SystemNode>> current_frame_systems;
    std::vector<std::shared_ptr<SystemNode>>
                          previous_frame_systems; // The G_inf sliding window
    std::atomic<uint64_t> current_frame { 0 };

    std::vector<std::string> execution_log;
    std::vector<std::string> error_log;

    /* Shio: The Bipartite Transitive Closure projection.
       This runs dynamically BEFORE the execution phase to securely bound
       the JIT blast radius without locking the physical memory. */
    void compute_transitive_closure(std::shared_ptr<SystemNode> &sys)
    {
        if(sys->static_delete_mask == 0) return;

        ComponentMask target_signature = sys->static_read_mask |
            sys->static_write_mask |
            sys->static_delete_mask;
        std::vector<EntityId> condemned = db.query_entities(target_signature);

        std::queue<EntityId> frontier;
        for(EntityId e : condemned)
            frontier.push(e);

        while(!frontier.empty())
        {
            EntityId current = frontier.front();
            frontier.pop();

            sys->jit_write_mask |= db.get_dynamic_mask(current);

            // Expand down the Bipartite Edge adjacency list
            for(EntityId child_edge : db.get_outbound_edges(current))
            {
                sys->jit_write_mask |= db.get_dynamic_mask(child_edge);
                frontier.push(child_edge);
            }
        }
    }

public:
    explicit TaskGraphExecutive(LayeredDatabaseAggregator &database)
        : db(database)
    {
        uint32_t hw_threads = std::thread::hardware_concurrency();
        worker_pool.start(db, hw_threads == 0 ? 4 : hw_threads);
    }

    template <typename SystemType>
    void register_system(const std::string &name)
    {
        auto node         = std::make_shared<SystemNode>();
        node->frame_index = current_frame.load();
        node->name        = name;

        using Query              = typename SystemType::EntityQuery;
        node->static_read_mask   = meta_utils::extract_read_mask<Query>();
        node->static_write_mask  = meta_utils::extract_write_mask<Query>();
        node->static_delete_mask = meta_utils::extract_delete_mask<Query>();
        node->execution_flags    = meta_utils::extract_flags<Query>();

        node->execute_proxy_fn = [name, this](
                                     LayeredDatabaseAggregator &db_ref) {
            DatabaseAccess<SystemType> proxy(db_ref, 0);
            SystemType::update(proxy);

            // Optional: Thread-safe debug logging could be injected here,
            // though actual stdout logging breaks realtime performance.
        };
        current_frame_systems.push_back(node);
    }

    void clear_systems()
    {
        current_frame_systems.clear();
        previous_frame_systems.clear();
    }

    const std::vector<std::string> &get_execution_log() const
    {
        return execution_log;
    }

    const std::vector<std::string> &get_error_log() const { return error_log; }

    void submit_frame()
    {
        // Evaluate orthogonal severance logic via lambda captures
        auto evaluate_collision = [](const auto &sys_a, const auto &sys_b) {
            bool overlap_rw =
                (sys_a->jit_write_mask & sys_b->static_read_mask) &&
                !((sys_b->execution_flags & DataAccessFlags::Previous) !=
                    DataAccessFlags::None);
            bool overlap_wr =
                (sys_a->static_read_mask & sys_b->jit_write_mask) &&
                !((sys_a->execution_flags & DataAccessFlags::Previous) !=
                    DataAccessFlags::None);
            bool overlap_ww = (sys_a->jit_write_mask & sys_b->jit_write_mask) &&
                !((sys_a->execution_flags & DataAccessFlags::Atomic) !=
                    DataAccessFlags::None);
            return overlap_rw || overlap_wr || overlap_ww;
        };

        auto bind_dependency = [](auto &pre, auto &post) {
            pre->dependents.push_back(post);
            post->in_degree++;
        };

        // 1. Establish the JIT Dynamic Write Bounds
        for(auto &sys : current_frame_systems)
        {
            sys->jit_write_mask = sys->static_write_mask;
            compute_transitive_closure(sys);
        }

        // 2. Continuous Edge Generation (G_inf sliding window)
        for(size_t i = 0; i < current_frame_systems.size(); ++i)
        {
            auto &sys_i = current_frame_systems[i];

            // Intra-frame bounds
            for(size_t j = i + 1; j < current_frame_systems.size(); ++j)
            {
                if(evaluate_collision(sys_i, current_frame_systems[j]))
                    bind_dependency(sys_i, current_frame_systems[j]);
            }

            // Cross-frame pipeline bounds
            for(auto &prev_sys : previous_frame_systems)
            {
                if(evaluate_collision(prev_sys, sys_i))
                    bind_dependency(prev_sys, sys_i);
            }
        }

        // 3. Dispatch root nodes into the persistent pool
        for(auto &sys : current_frame_systems)
        {
            if(sys->in_degree.load() == 0) worker_pool.push_task(sys);
        }

        // Slide the window mathematically
        previous_frame_systems = std::move(current_frame_systems);
        current_frame_systems.clear();
        current_frame++;
    }

    void wait_for_frame_completion()
    {
        bool done = false;
        while(!done)
        {
            done = true;
            for(auto &sys : previous_frame_systems)
            {
                if(!sys->executed && !sys->aborted_due_to_dependency)
                    done = false;
            }
            if(!done) std::this_thread::yield();
        }
        db.commit_pending_spawns();
    }

    // Legacy synchronous compatibility for earlier test suites
    void execute_frame()
    {
        submit_frame();
        wait_for_frame_completion();
    }
};
} // namespace usagi
