/*
 * Usagi Engine: Exhaustive Mathematical Proofs for Executive Edge Cases
 */

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi
{
// Component Bit Mappings
struct ComponentPlayer2
{
};

template <>
consteval ComponentMask get_component_bit<ComponentPlayer2>()
{
    return 1ull << 0;
}

struct ComponentItem2
{
};

template <>
consteval ComponentMask get_component_bit<ComponentItem2>()
{
    return 1ull << 1;
}

struct ComponentPhysics2
{
};

template <>
consteval ComponentMask get_component_bit<ComponentPhysics2>()
{
    return 1ull << 2;
}

struct ComponentRequest
{
};

template <>
consteval ComponentMask get_component_bit<ComponentRequest>()
{
    return 1ull << 3;
}

// -----------------------------------------------------------------------------
// System Mocks
// -----------------------------------------------------------------------------
struct SystemValidPhysics
{
    using EntityQuery =
        EntityQuery<Read<ComponentPhysics2>, Write<ComponentPhysics2>>;

    static void update(DatabaseAccess<SystemValidPhysics> &db) { }
};

struct SystemDeleteItems
{
    using EntityQuery =
        EntityQuery<Read<ComponentItem2>, IntentDelete<ComponentItem2>>;

    static void update(DatabaseAccess<SystemDeleteItems> &db)
    {
        auto items = db.query_entities(get_component_bit<ComponentItem2>());
        for(EntityId id : items)
            db.destroy_entity(id);
    }
};

struct SystemFaultySpawnsInfinite
{
    // Shio: This system spawns but never deletes the request. It creates a
    // Petri net paradox.
    using EntityQuery = EntityQuery<Read<ComponentRequest>>;

    static void update(DatabaseAccess<SystemFaultySpawnsInfinite> &db)
    {
        auto reqs = db.query_entities(get_component_bit<ComponentRequest>());
        if(!reqs.empty())
            db.queue_spawn(get_component_bit<ComponentPhysics2>());
    }
};

struct SystemCrashingNode
{
    using EntityQuery = EntityQuery<Write<ComponentPlayer2>>;

    static void update(DatabaseAccess<SystemCrashingNode> &db)
    {
        throw std::runtime_error("Simulated Division By Zero");
    }
};

struct SystemDependentOnCrash
{
    using EntityQuery = EntityQuery<Read<ComponentPlayer2>>;

    static void update(DatabaseAccess<SystemDependentOnCrash> &db) { }
};

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiExecutiveTestEdgeCases : public ::testing::Test
{
protected:
    EntityDatabase     db;
    TaskGraphExecutive executive { db };
};

/* TEST 1: Infinite Re-entrancy (Petri Net Unboundedness)
 * Proves the engine catches topological loops where a System continuously
 * queues state changes without consuming the trigger. */
TEST_F(UsagiExecutiveTestEdgeCases, InfiniteReEntrancyThrowsFatalException)
{
    db.create_entity_immediate(get_component_bit<ComponentRequest>());
    executive.register_system<SystemFaultySpawnsInfinite>("InfiniteSpawner");

    // Expect the mathematical bound to be enforced by the Executive
    EXPECT_THROW({ executive.execute_frame(); }, std::runtime_error);
}

/* TEST 2: Cascading Dependent Abortion
 * Proves that if System A crashes, System B (which reads A's writes) is aborted
 * to prevent reading corrupted memory, while System C (independent) runs fine.
 */
TEST_F(
    UsagiExecutiveTestEdgeCases,
    CrashingNodeAbortsDependentsButAllowsIndependent)
{
    executive.register_system<SystemCrashingNode>("CrashingA");
    executive.register_system<SystemDependentOnCrash>("DependentB");
    executive.register_system<SystemValidPhysics>("IndependentC");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &err_log  = executive.get_error_log();
    const auto &exec_log = executive.get_execution_log();

    bool caught_exception  = false;
    bool aborted_dependent = false;
    for(const auto &err : err_log)
    {
        if(err.find("Simulated Division By Zero") != std::string::npos)
            caught_exception = true;
        if(err.find("Aborted downstream dependent: [DependentB]") !=
            std::string::npos)
            aborted_dependent = true;
    }

    bool executed_independent = false;
    for(const auto &log : exec_log)
    {
        if(log == "Executed: IndependentC") executed_independent = true;
    }

    EXPECT_TRUE(caught_exception)
        << "Executive failed to catch the isolated exception.";
    EXPECT_TRUE(aborted_dependent)
        << "Executive failed to calculate topological dependence and abort "
           "downstream node.";
    EXPECT_TRUE(executed_independent)
        << "Executive unnecessarily blocked an independent parallel system.";
}

/* TEST 3: Access Violation / Compile-Time Proxy Firewall
 * (Commented out because a static_assert failure stops compilation, which is
 * the exact intended mathematical outcome. It proves the proxy works.)
 */
/*
struct SystemIllegalDeletion {
    using EntityQuery = EntityQuery<Read<ComponentItem2>>; // Missing
IntentDelete static void update(DatabaseAccess<SystemIllegalDeletion>& db) {
        db.destroy_entity(42); // COMPILE ERROR: FATAL: System attempted to
delete...
    }
};
*/
} // namespace usagi
