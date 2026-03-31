/*
 * Usagi Engine: Exhaustive Mathematical Proofs for Pathological Topologies
 * -----------------------------------------------------------------------------
 * Asserts the Executive's resilience against mathematically extreme graph
 * configurations: deep linear depths (Stack Overflows), complete bipartite
 * avalanches (Condition Variable storms), Ghost nodes, and side-effect leakage.
 */

#include <atomic>
#include <string>
#include <unordered_map>

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi::poc::executive_1
{
// -----------------------------------------------------------------------------
// Component Definitions & Bit Mappings
// -----------------------------------------------------------------------------
struct PathoCompA
{
};

template <>
consteval ComponentMask get_component_bit<PathoCompA>()
{
    return 1ull << 30;
}

struct PathoCompB
{
};

template <>
consteval ComponentMask get_component_bit<PathoCompB>()
{
    return 1ull << 31;
}

struct PathoCompC
{
};

template <>
consteval ComponentMask get_component_bit<PathoCompC>()
{
    return 1ull << 32;
}

// -----------------------------------------------------------------------------
// System Mocks
// -----------------------------------------------------------------------------

/* Shio: The Deep Recursion Generator. A template structure that builds a
   strict mathematical chain of dependencies S(N) -> S(N+1). */
template <size_t N>
struct SysDeepChainNode
{
    // Reads A, Writes B to force serialization
    using EntityQuery = EntityQuery<Read<PathoCompA>, Write<PathoCompB>>;

    static void update(DatabaseAccess<SysDeepChainNode> &) { }
};

struct SysCrashRoot
{
    using EntityQuery = EntityQuery<Write<PathoCompA>>;

    static void update(DatabaseAccess<SysCrashRoot> &)
    {
        throw std::runtime_error("Root Fracture");
    }
};

struct SysBipartiteReader
{
    using EntityQuery = EntityQuery<Read<PathoCompA>>;

    static void update(DatabaseAccess<SysBipartiteReader> &) { }
};

struct SysBipartiteWriter
{
    using EntityQuery = EntityQuery<Write<PathoCompA>>;

    static void update(DatabaseAccess<SysBipartiteWriter> &) { }
};

struct SysGhostNode
{
    // Queries NOTHING, but declares IntentDelete.
    // Mathematical implication: Blast radius is the Universal Set of the
    // database.
    using EntityQuery = EntityQuery<IntentDelete<PathoCompC>>;

    static void update(DatabaseAccess<SysGhostNode> &db)
    {
        auto ents = db.query_entities(0); // Query all
        for(auto e : ents)
            db.destroy_entity(e);
    }
};

struct SysKamikazeSpawner
{
    using EntityQuery = EntityQuery<Read<PathoCompA>>;

    static void update(DatabaseAccess<SysKamikazeSpawner> &db)
    {
        db.queue_spawn(get_component_bit<PathoCompB>());
        throw std::runtime_error("Kamikaze Exception");
    }
};

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiExecutivePathologicalTest : public ::testing::Test
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

// --- PATHOLOGICAL TOPOLOGY PROOFS ---

TEST_F(
    UsagiExecutivePathologicalTest,
    Pathological_DeepLinearDependency_NoStackOverflow)
{
    // Constructs a DAG of depth 10,000. S0 -> S1 -> S2...
    // The root node throws an exception. If abortion is implemented
    // recursively, the OS will fire SIGSEGV (Stack Overflow). The test
    // validates the implementation of iterative DAG traversal.

    executive.register_system<SysCrashRoot>("Root");

    // We simulate 10,000 nodes using a proxy loop to avoid 10,000 template
    // instantiations which would crash the compiler's template depth limit.
    for(int i = 0; i < 10'000; ++i)
    {
        executive.register_system<SysDeepChainNode<0>>(
            "ChainNode_" + std::to_string(i));
    }

    // The frame must complete without blowing the call stack.
    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &error_log = executive.get_error_log();

    // 1 root exception + 10,000 aborts = 10,001 log entries.
    EXPECT_EQ(error_log.size(), 10'001)
        << "Executive failed to traverse the massive graph iteratively.";
}

TEST_F(UsagiExecutivePathologicalTest, Pathological_CompleteBipartiteContention)
{
    // Tests thread pool and std::condition_variable starvation/wake-up storms.
    // 100 Readers registered before 100 Writers forms 10,000 topological edges.

    for(int i = 0; i < 100; ++i)
    {
        executive.register_system<SysBipartiteReader>(
            "Reader_" + std::to_string(i));
    }
    for(int i = 0; i < 100; ++i)
    {
        executive.register_system<SysBipartiteWriter>(
            "Writer_" + std::to_string(i));
    }

    EXPECT_NO_THROW({ executive.execute_frame(); });

    // Validates that all 200 nodes executed without hanging the thread pool
    EXPECT_EQ(executive.get_execution_log().size(), 200);
}

TEST_F(UsagiExecutivePathologicalTest, Pathological_TheGhostNode_GlobalLock)
{
    // A system with an empty query but an IntentDelete evaluates to a
    // mathematical Universal Set. The JIT escalator must lock the entire
    // database and force sequential evaluation.

    // Entity with A, Entity with B.
    db.create_entity_immediate(get_component_bit<PathoCompA>());
    db.create_entity_immediate(get_component_bit<PathoCompB>());

    executive.register_system<SysBipartiteReader>("ReaderA"); // Operates on A
    executive.register_system<SysGhostNode>(
        "GhostDeleter"); // Operates on NOTHING, deletes C
    executive.register_system<SysBipartiteWriter>("WriterA"); // Operates on A

    executive.execute_frame();

    const auto &log = executive.get_execution_log();
    ASSERT_EQ(log.size(), 3);

    // Topology must be: ReaderA -> GhostDeleter -> WriterA
    // The Ghost node expands its write lock to cover A and B because they exist
    // in the DB.
    int r_idx = -1, g_idx = -1, w_idx = -1;
    for(size_t i = 0; i < log.size(); ++i)
    {
        if(log[i] == "Executed: ReaderA") r_idx = i;
        if(log[i] == "Executed: GhostDeleter") g_idx = i;
        if(log[i] == "Executed: WriterA") w_idx = i;
    }

    EXPECT_LT(r_idx, g_idx)
        << "Ghost node failed to serialize against previous reader.";
    EXPECT_LT(g_idx, w_idx)
        << "Ghost node failed to serialize against subsequent writer.";

    // Verify Ghost Node actually mathematically obliterated the database
    EXPECT_EQ(db.query_entities(0).size(), 0)
        << "Ghost node failed to execute universal deletion.";
}

TEST_F(
    UsagiExecutivePathologicalTest, Pathological_KamikazeSpawn_CommitsOnCrash)
{
    // Proves that structural side-effects executed BEFORE an exception is
    // thrown are correctly preserved by the endomorphism.

    db.create_entity_immediate(get_component_bit<PathoCompA>());

    executive.register_system<SysKamikazeSpawner>("Kamikaze");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    // The system failed
    EXPECT_EQ(executive.get_error_log().size(), 1);

    // BUT the structural queue from the aborted node was flushed at the end of
    // the frame.
    auto spawned_ents = db.query_entities(get_component_bit<PathoCompB>());
    EXPECT_EQ(spawned_ents.size(), 1) << "Executive dropped deferred mutations "
                                         "generated before an exception fault.";
}
} // namespace usagi::poc::executive_1
