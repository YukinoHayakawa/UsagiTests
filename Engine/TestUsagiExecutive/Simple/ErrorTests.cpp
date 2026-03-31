/*
 * Usagi Engine: Exhaustive Mathematical Proofs for Executive Error & Edge Cases
 * -----------------------------------------------------------------------------
 * Validates SEH exception firewalls, infinite topological loops, deep cascading
 * dependency abortions, and empty/zero-radius bounding states.
 */

#include <unordered_map>

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi::poc::executive_1
{
// -----------------------------------------------------------------------------
// Component Definitions & Bit Mappings
// -----------------------------------------------------------------------------
struct ComponentNetworkState
{
    int status { 0 };
};

template <>
consteval ComponentMask get_component_bit<ComponentNetworkState>()
{
    return 1ull << 0;
}

struct ComponentAIState
{
    int behavior_tree_id { 0 };
};

template <>
consteval ComponentMask get_component_bit<ComponentAIState>()
{
    return 1ull << 1;
}

struct ComponentHealth
{
    int hp { 100 };
};

template <>
consteval ComponentMask get_component_bit<ComponentHealth>()
{
    return 1ull << 2;
}

struct ComponentInfiniteLoopTrigger
{
    int dummy { 0 };
};

template <>
consteval ComponentMask get_component_bit<ComponentInfiniteLoopTrigger>()
{
    return 1ull << 3;
}

struct ComponentCountdownToken
{
    int cycles_remaining { 0 };
};

template <>
consteval ComponentMask get_component_bit<ComponentCountdownToken>()
{
    return 1ull << 4;
}

// -----------------------------------------------------------------------------
// Mock Sparse Matrix Memory Chunks
// -----------------------------------------------------------------------------
std::unordered_map<EntityId, ComponentHealth>         g_healths;
std::unordered_map<EntityId, ComponentCountdownToken> g_countdowns;

void clear_error_mock_memory()
{
    g_healths.clear();
    g_countdowns.clear();
}

// -----------------------------------------------------------------------------
// System Mocks
// -----------------------------------------------------------------------------

struct SystemValidHealthRegen
{
    using EntityQuery =
        EntityQuery<Read<ComponentHealth>, Write<ComponentHealth>>;

    static void update(DatabaseAccess<SystemValidHealthRegen> &db)
    {
        auto ents = db.query_entities(build_mask<ComponentHealth>());
        for(EntityId id : ents)
            g_healths[id].hp += 10;
    }
};

struct SystemDeleteDeadAI
{
    using EntityQuery =
        EntityQuery<Read<ComponentAIState>, IntentDelete<ComponentAIState>>;

    static void update(DatabaseAccess<SystemDeleteDeadAI> &db)
    {
        // Will be used to test 0-blast-radius edge cases
    }
};

struct SystemFaultySpawnsInfinite
{
    // Shio: Spawns but never consumes the token. Causes unbounded Petri net.
    using EntityQuery = EntityQuery<Read<ComponentInfiniteLoopTrigger>>;

    static void update(DatabaseAccess<SystemFaultySpawnsInfinite> &db)
    {
        auto reqs =
            db.query_entities(build_mask<ComponentInfiniteLoopTrigger>());
        if(!reqs.empty()) db.queue_spawn(build_mask<ComponentAIState>());
    }
};

struct SystemBoundedNetworkRetry
{
    // Yukino: Consumes one token, mutates its data, and respawns if limit
    // allows.
    using EntityQuery = EntityQuery<
        Read<ComponentCountdownToken>, IntentDelete<ComponentCountdownToken>
    >;

    static void update(DatabaseAccess<SystemBoundedNetworkRetry> &db)
    {
        auto ents = db.query_entities(build_mask<ComponentCountdownToken>());
        for(EntityId id : ents)
        {
            int remaining = g_countdowns[id].cycles_remaining;
            db.destroy_entity(id);
            g_countdowns.erase(id);

            if(remaining > 0)
            {
                db.queue_spawn(build_mask<ComponentCountdownToken>());
                // Note: The data mapping for the newly spawned entity isn't
                // easily done mid-frame without a robust structural command
                // buffer. For this test, we just rely on the structural limits
                // being enforced.
            }
        }
    }
};

struct SystemCrashingNetwork
{
    using EntityQuery = EntityQuery<Write<ComponentNetworkState>>;

    static void update(DatabaseAccess<SystemCrashingNetwork> &db)
    {
        throw std::runtime_error("Simulated TCP Socket Disconnect");
    }
};

struct SystemCrashingAI
{
    using EntityQuery = EntityQuery<Write<ComponentAIState>>;

    static void update(DatabaseAccess<SystemCrashingAI> &db)
    {
        throw std::runtime_error("Simulated Behavior Tree Fault");
    }
};

struct SystemDependentOnNetwork
{
    using EntityQuery = EntityQuery<Read<ComponentNetworkState>>;

    static void update(DatabaseAccess<SystemDependentOnNetwork> &db) { }
};

struct SystemDependentOnDependentNetwork
{
    using EntityQuery = EntityQuery<Write<ComponentNetworkState>>;

    static void update(DatabaseAccess<SystemDependentOnDependentNetwork> &db) {
    }
};

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiExecutiveErrorTest : public ::testing::Test
{
protected:
    EntityDatabase     db;
    TaskGraphExecutive executive { db };

    void SetUp() override
    {
        db.clear_database();
        executive.clear_systems();
        clear_error_mock_memory();
    }
};

// --- FAILURE & EDGE CASES ---

TEST_F(UsagiExecutiveErrorTest, Failure_InfiniteReEntrancy_ThrowsException)
{
    db.create_entity_immediate(build_mask<ComponentInfiniteLoopTrigger>());
    executive.register_system<SystemFaultySpawnsInfinite>("InfiniteSpawner");

    // Expect the mathematical bound to be enforced by the Executive SEH
    EXPECT_THROW({ executive.execute_frame(); }, std::runtime_error);
}

TEST_F(UsagiExecutiveErrorTest, Edge_ReEntrancyBounded_ResolvesSafely)
{
    // Provide a valid loop bounds that does not exceed MAX_RE_ENTRIES
    EntityId e =
        db.create_entity_immediate(build_mask<ComponentCountdownToken>());
    g_countdowns[e] = { MAX_RE_ENTRIES - 2 };

    executive.register_system<SystemBoundedNetworkRetry>("BoundedRetry");

    EXPECT_NO_THROW({ executive.execute_frame(); });
}

TEST_F(
    UsagiExecutiveErrorTest,
    Failure_IsolatedNodeException_AllowsIndependentDataMutation)
{
    EntityId e   = db.create_entity_immediate(build_mask<ComponentHealth>());
    g_healths[e] = { 50 };

    executive.register_system<SystemCrashingNetwork>("CrashingNetwork");
    executive.register_system<SystemValidHealthRegen>("HealthRegen");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &exec_log  = executive.get_execution_log();
    bool        regen_ran = false;
    for(const auto &log : exec_log)
    {
        if(log == "Executed: HealthRegen") regen_ran = true;
    }
    EXPECT_TRUE(regen_ran)
        << "Isolated exception illegally halted parallel topology.";

    // Prove that the independent system actually successfully committed memory
    // mutations
    EXPECT_EQ(g_healths[e].hp, 60);
}

TEST_F(
    UsagiExecutiveErrorTest, Failure_DeepCascadingDependency_AbortsDownstream)
{
    // Register chain: Crash -> Dependent1 -> Dependent2
    executive.register_system<SystemCrashingNetwork>("CrashingNetwork");
    executive.register_system<SystemDependentOnNetwork>("DependentA");
    executive.register_system<SystemDependentOnDependentNetwork>("DependentB");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &exec_log  = executive.get_execution_log();
    bool        aborted_a = false, aborted_b = false;
    for(const auto &log : exec_log)
    {
        if(log == "ABORTED: DependentA") aborted_a = true;
        if(log == "ABORTED: DependentB") aborted_b = true;
    }
    EXPECT_TRUE(aborted_a && aborted_b)
        << "Failed to resolve deep transitive abortion chain to protect memory "
           "reads.";
}

TEST_F(UsagiExecutiveErrorTest, Failure_MultipleIndependentCrashes)
{
    executive.register_system<SystemCrashingNetwork>("CrashingNetwork");
    executive.register_system<SystemCrashingAI>("CrashingAI");

    EntityId e   = db.create_entity_immediate(build_mask<ComponentHealth>());
    g_healths[e] = { 100 };
    executive.register_system<SystemValidHealthRegen>("SafeHealth");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &errors = executive.get_error_log();
    ASSERT_EQ(errors.size(), 2)
        << "Executive failed to catch multiple disparate SEH faults.";

    EXPECT_EQ(g_healths[e].hp, 110)
        << "Safe parallel memory mutation failed under multi-fault stress.";
}

TEST_F(UsagiExecutiveErrorTest, Edge_EmptyGraph_NoCrash)
{
    EXPECT_NO_THROW({ executive.execute_frame(); });
    EXPECT_TRUE(executive.get_execution_log().empty());
}

TEST_F(UsagiExecutiveErrorTest, Edge_JITDeleteNonExistent_NoEscalation)
{
    // Database has NO AI entities. Blast radius is exactly 0.
    EntityId hp_ent = db.create_entity_immediate(build_mask<ComponentHealth>());
    g_healths[hp_ent] = { 10 };

    executive.register_system<SystemDeleteDeadAI>("DeleteDeadAI");
    executive.register_system<SystemValidHealthRegen>("HealthRegen");

    executive.execute_frame();

    const auto &log = executive.get_execution_log();
    ASSERT_EQ(log.size(), 2);

    // Ensure both ran safely without topological deadlock despite the
    // IntentDelete declaration resolving to an empty set.
    EXPECT_EQ(g_healths[hp_ent].hp, 20);
}
} // namespace usagi::poc::executive_1
