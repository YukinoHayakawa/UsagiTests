#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi
{
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

/* Yukino: A transient request component. Systems that trigger re-entrancy
   must consume these to prevent infinite topological loops. */
struct ComponentRayCastRequest
{
    float distance;
};

template <>
consteval ComponentMask get_component_bit<ComponentRayCastRequest>()
{
    return 1ull << 4;
}

// -----------------------------------------------------------------------------
// Concrete System Implementations (For Testing)
// -----------------------------------------------------------------------------

struct SystemPhysicsCompute
{
    using EntityQuery =
        EntityQuery<Read<ComponentPhysics>, Write<ComponentPhysics>>;

    static void update(DatabaseAccess<SystemPhysicsCompute> &db)
    {
        // Logic: Move objects. No structural changes.
    }
};

struct SystemDeleteEmptyItems
{
    // Declares intent to delete an entity that has ComponentItem
    using EntityQuery =
        EntityQuery<Read<ComponentItem>, IntentDelete<ComponentItem>>;

    static void update(DatabaseAccess<SystemDeleteEmptyItems> &db)
    {
        auto items = db.query_entities(get_component_bit<ComponentItem>());
        for(EntityId id : items)
        {
            db.destroy_entity(id);
        }
    }
};

/* Shio: The system triggering re-entrancy must consume its triggering condition
 * (the RayCastRequest) to prevent the Task Graph from looping infinitely. */
struct SystemRayTracerSpawner
{
    using EntityQuery = EntityQuery<
        Read<ComponentRayCastRequest>, IntentDelete<ComponentRayCastRequest>
    >;

    static void update(DatabaseAccess<SystemRayTracerSpawner> &db)
    {
        auto requests =
            db.query_entities(get_component_bit<ComponentRayCastRequest>());
        for(EntityId id : requests)
        {
            // Spawns a transient hit-marker entity
            db.queue_spawn(get_component_bit<ComponentPhysics>());
            // Consume the request token to satisfy Petri net bounds
            db.destroy_entity(id);
        }
    }
};

struct SystemCrashingService
{
    using EntityQuery = EntityQuery<Read<ComponentInventory>>;

    static void update(DatabaseAccess<SystemCrashingService> &db)
    {
        throw std::runtime_error("Vulkan Device Lost!");
    }
};

// -----------------------------------------------------------------------------
// Mathematical Proofs via Google Test
// -----------------------------------------------------------------------------

class UsagiExecutiveTestNormal : public ::testing::Test
{
protected:
    EntityDatabase     db;
    TaskGraphExecutive executive { db };
};

// Test 1: Static DAG Execution (Parallelism achievable)
TEST_F(UsagiExecutiveTestNormal, StaticTaskGraphExecution)
{
    db.create_entity_immediate(get_component_bit<ComponentPhysics>());

    executive.register_system<SystemPhysicsCompute>("PhysicsCompute");
    executive.execute_frame();

    const auto &log = executive.get_execution_log();
    ASSERT_EQ(log.size(), 1);
    EXPECT_EQ(log[0], "Executed: PhysicsCompute");
}

// Test 2: Just-In-Time Lock Escalation
TEST_F(UsagiExecutiveTestNormal, JITLockEscalationIsolatesPhysics)
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
TEST_F(UsagiExecutiveTestNormal, CyclicReEntrancyIsResolved)
{
    // Initialize the transient request rather than a persistent component
    db.create_entity_immediate(get_component_bit<ComponentRayCastRequest>());

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
TEST_F(UsagiExecutiveTestNormal, ExceptionFirewallPreventsEngineCrash)
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
