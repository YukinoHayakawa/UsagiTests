/*
 * Usagi Engine: Asymptotic & Singularity Proofs
 * -----------------------------------------------------------------------------
 * Subjecting the Task Graph Executive and EntityDatabase to extreme boundary
 * conditions: Integer exhaustion, Out-Of-Memory (OOM) allocator exceptions,
 * graph density saturation, structural bitmask thrashing, and null-mask
 * allocations.
 */

#include <iostream>
#include <new> // For std::bad_alloc
#include <thread>

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi::poc::executive_1
{
// -----------------------------------------------------------------------------
// Component Definitions & Bit Mappings
// -----------------------------------------------------------------------------
struct CompSingularityA
{
    int data { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompSingularityA>()
{
    return 1ull << 60;
}

struct CompSingularityB
{
    int data { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompSingularityB>()
{
    return 1ull << 61;
}

// Yukino: State token to mathematically bound the Void Spawner and prevent
// infinite loops.
struct CompVoidSpawnerToken
{
    int dummy { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompVoidSpawnerToken>()
{
    return 1ull << 62;
}

struct CompThrashA
{
};

template <>
consteval ComponentMask get_component_bit<CompThrashA>()
{
    return 1ull << 50;
}

struct CompThrashB
{
};

template <>
consteval ComponentMask get_component_bit<CompThrashB>()
{
    return 1ull << 51;
}

// -----------------------------------------------------------------------------
// System Mocks
// -----------------------------------------------------------------------------
struct SysIdExhaustionSpammer
{
    using EntityQuery = EntityQuery<Write<CompSingularityA>>;

    static void update(DatabaseAccess<SysIdExhaustionSpammer> &db)
    {
        // Spawns entities to forcefully trigger the uint32_t boundary limit
        for(int i = 0; i < 5; ++i)
        {
            db.queue_spawn(build_mask<CompSingularityA>());
        }
    }
};

struct SysBadAllocThrower
{
    using EntityQuery = EntityQuery<Write<CompSingularityA>>;

    static void update(DatabaseAccess<SysBadAllocThrower> &)
    {
        // Mathematically simulates a std::vector reallocation failure
        // when orthogonal partitions saturate physical memory bounds.
        throw std::bad_alloc();
    }
};

struct SysDependentOnBadAlloc
{
    using EntityQuery = EntityQuery<Read<CompSingularityA>>;

    static void update(DatabaseAccess<SysDependentOnBadAlloc> &) { }
};

struct SysVoidSpawner
{
    using EntityQuery = EntityQuery<
        Read<CompVoidSpawnerToken>, IntentDelete<CompVoidSpawnerToken>
    >;

    static void update(DatabaseAccess<SysVoidSpawner> &db)
    {
        auto ents = db.query_entities(build_mask<CompVoidSpawnerToken>());
        for(auto e : ents)
        {
            // Consume the state token to satisfy Petri net bounds.
            db.destroy_entity(e);

            // Queue an entity with a component mask of strictly 0.
            // It exists, but possesses no data footprint.
            db.queue_spawn(0);
        }
    }
};

// --- Savage Structural Thrashing Mocks ---

struct SysSavageAdder
{
    // Topologically locks ThrashB while querying ThrashA
    using EntityQuery = EntityQuery<Read<CompThrashA>, IntentAdd<CompThrashB>>;

    static void update(DatabaseAccess<SysSavageAdder> &db)
    {
        auto ents = db.query_entities(build_mask<CompThrashA>());
        for(auto e : ents)
        {
            db.add_component(e, build_mask<CompThrashB>());
            std::this_thread::yield();
        }
    }
};

struct SysSavageRemover
{
    // Topologically locks ThrashA while querying ThrashB
    using EntityQuery =
        EntityQuery<Read<CompThrashB>, IntentRemove<CompThrashA>>;

    static void update(DatabaseAccess<SysSavageRemover> &db)
    {
        auto ents = db.query_entities(build_mask<CompThrashB>());
        for(auto e : ents)
        {
            db.remove_component(e, build_mask<CompThrashA>());
            std::this_thread::yield();
        }
    }
};

struct SysSavageQuerySpammer
{
    // Continually queries the intersection. Operates lock-free against the
    // mutators because it only reads the atomic bitmask without locking the
    // data chunks.
    using EntityQuery = EntityQuery<
        Read<CompThrashA, CompThrashB>, AccessPolicy<DataAccessFlags::Previous>
    >;

    static void update(DatabaseAccess<SysSavageQuerySpammer> &db)
    {
        for(int i = 0; i < 50; ++i)
        {
            volatile auto ents =
                db.query_entities(build_mask<CompThrashA, CompThrashB>());
            (void)ents;
            std::this_thread::yield();
        }
    }
};

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiExecutiveSingularityTest : public ::testing::Test
{
protected:
    EntityDatabase     db;
    TaskGraphExecutive executive { db };

    void SetUp() override
    {
        db.clear_database();
        executive.clear_systems();
    }
};

// --- ASYMPTOTIC BOUNDARY PROOFS ---

TEST_F(
    UsagiExecutiveSingularityTest,
    Singularity_IdExhaustion_ThrowsFatalException)
{
    // Force the allocator to the precipice of the 0xFFFFFFFF singularity.
    db.debug_set_next_id(INVALID_ENTITY - 1);

    executive.register_system<SysIdExhaustionSpammer>("Spammer");

    // Shio: ID Exhaustion happens in commit_pending_spawns(), which runs on the
    // main thread AFTER the SEH worker threads have joined. Thus, it correctly
    // bypasses the task graph firewall and violently halts the engine.
    EXPECT_THROW({ executive.execute_frame(); }, std::runtime_error);
}

TEST_F(UsagiExecutiveSingularityTest, Singularity_OomBadAlloc_SehFirewall)
{
    // Tests that hardware memory exhaustion (std::bad_alloc) is safely caught
    // by the SEH execution pipeline and correctly diffuses topological poison
    // without unwinding the thread pool.

    executive.register_system<SysBadAllocThrower>("OomNode");
    executive.register_system<SysDependentOnBadAlloc>("DependentNode");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &error_log = executive.get_error_log();
    ASSERT_GE(error_log.size(), 1);

    bool caught_bad_alloc = false;
    for(const auto &log : error_log)
    {
        // Yukino: Compiler ABIs vary. MSVC emits "bad allocation", Clang/GCC
        // emit "std::bad_alloc". We match against "alloc" to ensure
        // cross-platform mathematical validity.
        if(log.find("Exception in [OomNode]") != std::string::npos &&
            log.find("alloc") != std::string::npos)
        {
            caught_bad_alloc = true;
        }
    }

    EXPECT_TRUE(caught_bad_alloc) << "Executive SEH firewall failed to trap a "
                                     "catastrophic allocator failure.";

    const auto &exec_log          = executive.get_execution_log();
    bool        aborted_dependent = false;
    for(const auto &log : exec_log)
    {
        if(log == "ABORTED: DependentNode") aborted_dependent = true;
    }

    EXPECT_TRUE(aborted_dependent)
        << "Topological poison failed to diffuse after an OOM fault.";
}

TEST_F(UsagiExecutiveSingularityTest, Singularity_NullMaskAllocation_VoidEntity)
{
    // Tests that allocating an entity with a mathematical mask of 0 does not
    // crash the sparse matrix or trigger infinite loops during bitwise AND
    // queries.

    db.create_entity_immediate(build_mask<CompVoidSpawnerToken>());
    executive.register_system<SysVoidSpawner>("VoidSpawner");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    // The universal set query (mask == 0) should mathematically return the void
    // entity. We destroyed the 1 starting token and spawned exactly 1 void
    // entity.
    auto universal_ents = db.query_entities(0);
    EXPECT_EQ(universal_ents.size(), 1)
        << "Sparse matrix query solver failed to resolve the universal set "
           "mapping for a void entity.";
}

// --- SAVAGE STRUCTURAL THRASHING PROOFS ---

TEST_F(
    UsagiExecutiveSingularityTest,
    Singularity_StructuralThrashing_AtomicMaskIntegrity)
{
    // Populates 10,000 entities with random distributions of ThrashA and
    // ThrashB.
    for(int i = 0; i < 5'000; ++i)
        db.create_entity_immediate(build_mask<CompThrashA>());
    for(int i = 0; i < 5'000; ++i)
        db.create_entity_immediate(build_mask<CompThrashB>());

    // We flood the Task Graph with 150 conflicting systems.
    // The DAG sorter will mathematically unroll the WAW collisions due to
    // IntentAdd mapping to Write locks, but the QuerySpammer will evaluate
    // lock-free concurrently using the 'Previous' flag. If the
    // EntityRecord::mask is not strictly atomic with acquire/release barriers,
    // the read/modify/write collisions will trigger an immediate SIGSEGV or
    // ThreadSanitizer violation.
    for(int i = 0; i < 50; ++i)
    {
        executive.register_system<SysSavageAdder>("Adder_" + std::to_string(i));
        executive.register_system<SysSavageRemover>(
            "Remover_" + std::to_string(i));
        executive.register_system<SysSavageQuerySpammer>(
            "QuerySpammer_" + std::to_string(i));
    }

    EXPECT_NO_THROW({ executive.execute_frame(); });

    // The execution log must reflect exactly 150 completed mathematical
    // evaluations without a single data race.
    EXPECT_EQ(executive.get_execution_log().size(), 150)
        << "The atomic structural matrix shattered under savage multithreaded "
           "thrashing.";
}
} // namespace usagi::poc::executive_1
