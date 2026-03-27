/* * Usagi Engine: Task Graph Executive Proof-of-Concept
 * * Topology: Directed Acyclic Graph (DAG) evaluated JIT per-frame.
 * Resolution: Table-level locking via ComponentMasks.
 * Features:
 * - C++26 Static Reflection (P2996R13) for Query DSL unpacking.
 * - JIT Lock Escalation for immediate Entity destruction.
 * - Cyclic Re-entrancy via deferred spawn queues.
 * - SEH/Exception firewalls for fault-tolerant execution.
 */

#include <cstdint>
#include <functional>
#include <iostream>
#include <meta> // C++26 P2996
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

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

// Component Definitions & Bit Mappings
struct ComponentPlayer
{
    FixedString<32> name;
};

template <>
consteval ComponentMask get_component_bit<ComponentPlayer>()
{
    return 1ull << 0;
}

struct ComponentItem
{
    uint32_t item_id;
};

template <>
consteval ComponentMask get_component_bit<ComponentItem>()
{
    return 1ull << 1;
}

struct ComponentPhysics
{
    float velocity;
};

template <>
consteval ComponentMask get_component_bit<ComponentPhysics>()
{
    return 1ull << 2;
}

struct ComponentInventory
{
    uint32_t capacity;
};

template <>
consteval ComponentMask get_component_bit<ComponentInventory>()
{
    return 1ull << 3;
}

// -----------------------------------------------------------------------------
// Declarative Query DSL (C++26)
// -----------------------------------------------------------------------------

/* Yukino: We use P2996 reflection to unpack variadic templates into bitmasks at
 * compile-time. */
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
    std::function<void(EntityDatabase &)> update_fn;

    // Topological sort state
    int              in_degree { 0 };
    std::vector<int> dependents;
    bool             executed { false };
    bool             failed { false };
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

        node.update_fn = [](EntityDatabase &db_ref) {
            SystemType::update(db_ref);
        };

        systems.push_back(std::move(node));
    }

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
        int           re_entry_counter = 0;
        constexpr int MAX_RE_ENTRIES   = 5;

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
    /* Yukino: Phase 1: The Blast Radius calculation.
       This solves the intersection hazard dynamically without global locks. */
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

    /* Shio: Phase 2: Topological sorting based on JIT locks.
       If System A and System B do not overlap, no edge is created,
       meaning they can mathematically execute in parallel. */
    void build_topological_graph()
    {
        for(auto &sys : systems)
        {
            sys.in_degree = 0;
            sys.dependents.clear();
            sys.executed = false;
            sys.failed   = false;
        }

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
       are pushed to a lock-free thread pool. For this PoC, we execute
       sequentially to verify topological correctness and SEH/Exception
       isolation. */
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

            try
            {
                sys.update_fn(db);
                sys.executed = true;
                execution_log.push_back("Executed: " + sys.name);
            }
            catch(const std::exception &e)
            {
                // Fault-tolerant execution. System crashed, but engine
                // survives.
                sys.failed   = true;
                sys.executed = true; // Mark done so graph doesn't hang
                                     // permanently, but dependents might suffer
                error_log.push_back(
                    "SEH/Exception in [" + sys.name + "]: " + e.what());
                execution_log.push_back("FAILED: " + sys.name);
            }

            // Unblock dependents
            for(int dep_idx : sys.dependents)
            {
                systems[dep_idx].in_degree--;
                if(systems[dep_idx].in_degree == 0)
                {
                    ready_queue.push(dep_idx);
                }
            }
        }

        // Cycle detection check
        for(const auto &sys : systems)
        {
            if(!sys.executed)
            {
                throw std::runtime_error(
                    "FATAL: Topological deadlock detected in Task Graph.");
            }
        }
    }
};

// -----------------------------------------------------------------------------
// Concrete System Implementations (For Testing)
// -----------------------------------------------------------------------------

struct SystemPhysicsCompute
{
    using EntityQuery =
        EntityQuery<Read<ComponentPhysics>, Write<ComponentPhysics>>;

    static void update(EntityDatabase &db)
    {
        // Logic: Move objects. No structural changes.
    }
};

struct SystemDeleteEmptyItems
{
    // Declares intent to delete an entity that has ComponentItem
    using EntityQuery =
        EntityQuery<Read<ComponentItem>, IntentDelete<ComponentItem>>;

    static void update(EntityDatabase &db)
    {
        auto items = db.query_entities(get_component_bit<ComponentItem>());
        for(EntityId id : items)
        {
            db.destroy_entity_immediate(id);
        }
    }
};

struct SystemRayTracerSpawner
{
    using EntityQuery = EntityQuery<Read<ComponentPlayer>>;

    static void update(EntityDatabase &db)
    {
        // Spawns a transient hit-marker entity
        db.queue_spawn(get_component_bit<ComponentPhysics>());
    }
};

struct SystemCrashingService
{
    using EntityQuery = EntityQuery<Read<ComponentInventory>>;

    static void update(EntityDatabase &db)
    {
        throw std::runtime_error("Vulkan Device Lost!");
    }
};

// -----------------------------------------------------------------------------
// Mathematical Proofs via Google Test
// -----------------------------------------------------------------------------

class UsagiExecutiveTest : public ::testing::Test
{
protected:
    EntityDatabase     db;
    TaskGraphExecutive executive { db };
};

// Test 1: Static DAG Execution (Parallelism achievable)
TEST_F(UsagiExecutiveTest, StaticTaskGraphExecution)
{
    db.create_entity_immediate(get_component_bit<ComponentPhysics>());

    executive.register_system<SystemPhysicsCompute>("PhysicsCompute");
    executive.execute_frame();

    const auto &log = executive.get_execution_log();
    ASSERT_EQ(log.size(), 1);
    EXPECT_EQ(log[0], "Executed: PhysicsCompute");
}

// Test 2: Just-In-Time Lock Escalation
TEST_F(UsagiExecutiveTest, JITLockEscalationIsolatesPhysics)
{
    // Create an entity that is BOTH an Item and a Physics object.
    // This perfectly models the ComponentGroup intersection hazard.
    ComponentMask dual_mask = get_component_bit<ComponentItem>() |
        get_component_bit<ComponentPhysics>();
    db.create_entity_immediate(dual_mask);

    executive.register_system<SystemDeleteEmptyItems>("DeleteItems");
    executive.register_system<SystemPhysicsCompute>("PhysicsCompute");

    // The Executive must execute DeleteItems, dynamically realize that deleting
    // the item ALSO deletes a Physics component, and serialize PhysicsCompute
    // AFTER DeleteItems.
    executive.execute_frame();

    const auto &log = executive.get_execution_log();

    // Check that the JIT escalation occurred
    bool escalation_detected = false;
    for(const auto &msg : log)
    {
        if(msg.find("JIT Escalation [DeleteItems]") != std::string::npos)
        {
            escalation_detected = true;
        }
    }
    EXPECT_TRUE(escalation_detected)
        << "Executive failed to detect structural intersection hazard.";

    // Order must be strictly DeleteItems THEN PhysicsCompute due to topological
    // edge
    int del_idx = -1, phys_idx = -1;
    for(size_t i = 0; i < log.size(); ++i)
    {
        if(log[i] == "Executed: DeleteItems") del_idx = i;
        if(log[i] == "Executed: PhysicsCompute") phys_idx = i;
    }

    EXPECT_NE(del_idx, -1);
    EXPECT_NE(phys_idx, -1);
    EXPECT_LT(del_idx, phys_idx) << "Topological sort failed: Physics ran "
                                    "before or parallel to its destruction.";

    // Verify component is actually dead
    auto physics_ents =
        db.query_entities(get_component_bit<ComponentPhysics>());
    EXPECT_TRUE(physics_ents.empty());
}

// Test 3: Cyclic Re-entrancy via Deferred Queues
TEST_F(UsagiExecutiveTest, CyclicReEntrancyIsResolved)
{
    db.create_entity_immediate(get_component_bit<ComponentPlayer>());

    executive.register_system<SystemRayTracerSpawner>("RaySpawner");
    executive.register_system<SystemPhysicsCompute>(
        "PhysicsCompute"); // Should process the spawned entity

    executive.execute_frame();

    const auto &log = executive.get_execution_log();

    bool re_entry_detected = false;
    for(const auto &msg : log)
    {
        if(msg == "--- Executive: Re-entrancy cycle triggered ---")
        {
            re_entry_detected = true;
        }
    }

    EXPECT_TRUE(re_entry_detected)
        << "Executive failed to evaluate the spawn queue and re-enter.";

    // Check if the spawned entity now exists
    auto physics_ents =
        db.query_entities(get_component_bit<ComponentPhysics>());
    EXPECT_EQ(physics_ents.size(), 1)
        << "Transient entity was not committed to the database.";
}

// Test 4: Fault Tolerance (SEH/Exception Firewall)
TEST_F(UsagiExecutiveTest, ExceptionFirewallPreventsEngineCrash)
{
    db.create_entity_immediate(get_component_bit<ComponentInventory>());

    executive.register_system<SystemCrashingService>("CrashingSystem");
    executive.register_system<SystemPhysicsCompute>(
        "SafePhysics"); // Unrelated system

    // This should NOT throw an exception up to GTest. The Executive must catch
    // it.
    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &errors = executive.get_error_log();
    ASSERT_EQ(errors.size(), 1);
    EXPECT_TRUE(errors[0].find("Vulkan Device Lost!") != std::string::npos);

    const auto &exec_log         = executive.get_execution_log();
    bool        safe_physics_ran = false;
    for(const auto &msg : exec_log)
    {
        if(msg == "Executed: SafePhysics") safe_physics_ran = true;
    }

    EXPECT_TRUE(safe_physics_ran) << "Engine orchestrator halted entire graph "
                                     "due to isolated system failure.";
}
} // namespace usagi
