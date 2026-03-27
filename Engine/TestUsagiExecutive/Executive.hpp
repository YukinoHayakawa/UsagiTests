/*
 * Usagi Engine: Task Graph Executive & ECS Core (C++26)
 * -----------------------------------------------------------------------------
 * Topology: Registration-ordered DAG evaluated JIT per-frame.
 * Resolution: Table-level locking via ComponentMasks.
 * Safety: Compile-time capability proxy & Runtime SEH/Exception firewalls.
 * Features:
 * - C++26 Static Reflection (P2996R13) for Query DSL unpacking.
 * - JIT Lock Escalation for immediate Entity destruction.
 * - Cyclic Re-entrancy via deferred spawn queues.
 * - SEH/Exception firewalls for fault-tolerant execution.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <meta> // C++26 P2996R13
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace usagi
{
// -----------------------------------------------------------------------------
// Core Mathematical Primitives
// -----------------------------------------------------------------------------
using EntityId = uint32_t;
[[maybe_unused]]
constexpr EntityId INVALID_ENTITY = 0xFFFF'FFFF;

/* Shio: We represent the existence of a component as a single bit in a 64-bit
   integer. This maps the infinite sparse matrix columns into manageable
   discrete bitwise operations for the Task Graph's intersection tests. */
using ComponentMask = uint64_t;

constexpr int MAX_RE_ENTRIES = 5;

template <typename T>
consteval ComponentMask get_component_bit();

// -----------------------------------------------------------------------------
// Compile-Time Utilities & C++26 Meta-programming
// -----------------------------------------------------------------------------
template <size_t N>
struct FixedString
{
    char data[N] { };

    constexpr FixedString(const char (&str)[N])
    {
        for(size_t i = 0; i < N - 1; ++i)
            data[i] = str[i];
    }

    constexpr bool operator==(const FixedString<N> &other) const
    {
        for(size_t i = 0; i < N; ++i)
            if(data[i] != other.data[i]) return false;
        return true;
    }
};

template <typename T>
consteval ComponentMask get_component_bit();

// -----------------------------------------------------------------------------
// Declarative Query DSL (C++26)
// -----------------------------------------------------------------------------
template <typename... Ts>
consteval ComponentMask build_mask()
{
    return (0ull | ... | get_component_bit<Ts>());
}

template <typename... Ts>
struct Read
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = true;
    static constexpr bool          is_write  = false;
    static constexpr bool          is_delete = false;
};

template <typename... Ts>
struct Write
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = false;
    static constexpr bool          is_write  = true;
    static constexpr bool          is_delete = false;
};

template <typename... Ts>
struct IntentDelete
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = false;
    static constexpr bool          is_write  = false;
    static constexpr bool          is_delete = true;
};

template <typename... Args>
struct EntityQuery
{
    /* Yukino: Using P2996 ^^ operator to reflect type arguments into a meta
     * info array. This array is processed at compile-time to enforce access
     * boundaries. */
    static constexpr std::array<std::meta::info, sizeof...(Args)> infos = {
        ^^Args...
    };
};

namespace meta_utils
{
template <typename Query>
consteval ComponentMask extract_read_mask()
{
    ComponentMask m = 0;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(Arg::is_read) m |= Arg::mask;
    }
    return m;
}

template <typename Query>
consteval ComponentMask extract_write_mask()
{
    ComponentMask m = 0;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(Arg::is_write) m |= Arg::mask;
    }
    return m;
}

template <typename Query>
consteval ComponentMask extract_delete_mask()
{
    ComponentMask m = 0;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(Arg::is_delete) m |= Arg::mask;
    }
    return m;
}
} // namespace meta_utils

// -----------------------------------------------------------------------------
// Entity Database
// -----------------------------------------------------------------------------
class EntityDatabase
{
    struct EntityRecord
    {
        EntityId      id;
        ComponentMask mask;
        bool          alive;
    };

    std::vector<EntityRecord> entities;
    EntityId                  next_id = 1;

    // Deferred structural queues for re-entrancy
    std::vector<ComponentMask> spawn_queue;

public:
    EntityDatabase() = default;

    EntityId create_entity_immediate(ComponentMask initial_mask)
    {
        EntityId id = next_id++;
        entities.push_back({ id, initial_mask, true });
        return id;
    }

    void destroy_entity_immediate(EntityId id)
    {
        for(auto &rec : entities)
        {
            if(rec.id == id)
            {
                rec.alive = false;
                rec.mask  = 0; // Obliterate structural footprint
                return;
            }
        }
    }

    /* Shio: The query execution. Used by both Systems during logic, and the
       Executive during the JIT pre-pass. */
    std::vector<EntityId> query_entities(ComponentMask required_mask) const
    {
        std::vector<EntityId> results;
        for(const auto &rec : entities)
        {
            if(rec.alive && (rec.mask & required_mask) == required_mask)
            {
                results.push_back(rec.id);
            }
        }
        return results;
    }

    ComponentMask get_dynamic_mask(EntityId id) const
    {
        for(const auto &rec : entities)
        {
            if(rec.id == id && rec.alive) return rec.mask;
        }
        return 0;
    }

    /* Yukino: Modifying an entity's structure at runtime. This simulates an
       entity joining a new ComponentGroup dynamically. */
    void add_component_immediate(EntityId id, ComponentMask comp_mask)
    {
        for(auto &rec : entities)
        {
            if(rec.id == id && rec.alive) rec.mask |= comp_mask;
        }
    }

    // --- Re-entrancy specific ---
    void queue_spawn(ComponentMask initial_mask)
    {
        spawn_queue.push_back(initial_mask);
    }

    bool has_pending_spawns() const { return !spawn_queue.empty(); }

    void commit_pending_spawns()
    {
        for(auto mask : spawn_queue)
        {
            create_entity_immediate(mask);
        }
        spawn_queue.clear();
    }

    void clear_database()
    {
        entities.clear();
        spawn_queue.clear();
        next_id = 1;
    }
};

// -----------------------------------------------------------------------------
// Compile-Time Capability Proxy (DatabaseAccess)
// -----------------------------------------------------------------------------
/* Shio: This proxy acts as a mathematical firewall. It prevents Systems from
   executing structural changes they did not explicitly request in EntityQuery.
 */
template <typename SystemType>
class DatabaseAccess
{
    EntityDatabase &db_ref;
    using Query = typename SystemType::EntityQuery;

public:
    explicit DatabaseAccess(EntityDatabase &db)
        : db_ref(db)
    {
    }

    std::vector<EntityId> query_entities(ComponentMask mask) const
    {
        return db_ref.query_entities(mask);
    }

    void queue_spawn(ComponentMask initial_mask)
    {
        db_ref.queue_spawn(initial_mask);
    }

    void destroy_entity(EntityId id)
    {
        constexpr ComponentMask delete_mask =
            meta_utils::extract_delete_mask<Query>();
        static_assert(
            delete_mask != 0,
            "FATAL: System attempted to delete an entity, but IntentDelete was "
            "not declared in EntityQuery.");
        db_ref.destroy_entity_immediate(id);
    }
};

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

    // Topological sort state
    int              in_degree { 0 };
    std::vector<int> dependents;
    bool             executed { false };
    bool             failed { false };
    bool             aborted_due_to_dependency { false };
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
        execution_log.clear();
        error_log.clear();
        int re_entry_counter = 0;

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
            sys.in_degree = 0;
            sys.dependents.clear();
            sys.executed                  = false;
            sys.failed                    = false;
            sys.aborted_due_to_dependency = false;
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
                    // Conflict detected. System i must finish before System j.
                    systems[i].dependents.push_back(j);
                    systems[j].in_degree++;
                }
            }
        }
    }

    /* Yukino: Phase 3: Dispatch. In a real engine, systems with in_degree == 0
     * are pushed to a lock-free thread pool. For this PoC, we execute
     * sequentially to verify topological correctness and SEH/Exception
     * isolation. */
    void dispatch_tasks()
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

                abort_dependents_recursively(current);
            }
        }
    }

    void abort_dependents_recursively(int node_idx)
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
                abort_dependents_recursively(dep_idx);
            }
        }
    }
};
} // namespace usagi
