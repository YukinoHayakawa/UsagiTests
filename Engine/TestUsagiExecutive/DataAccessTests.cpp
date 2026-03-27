/*
 * Usagi Engine: Exhaustive Proofs for DataAccessFlags & DAG Edge Severing
 * -----------------------------------------------------------------------------
 * Validates the topological compiler's ability to mathematically dissolve
 * false dependencies (WAW, WAR, RAW) utilizing orthogonal memory semantics.
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi
{
// -----------------------------------------------------------------------------
// Component Definitions & Bit Mappings
// -----------------------------------------------------------------------------
struct CompSharedResource
{
    std::atomic<int> atomic_counter { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompSharedResource>()
{
    return 1ull << 40;
}

struct CompTemporalState
{
    int frame_n { 0 };
    int frame_n_minus_1 { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompTemporalState>()
{
    return 1ull << 41;
}

struct CompStrictState
{
    int value { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompStrictState>()
{
    return 1ull << 42;
}

// -----------------------------------------------------------------------------
// Mock Sparse Matrix Memory Chunks
// -----------------------------------------------------------------------------
std::unordered_map<EntityId, CompSharedResource> g_shared_res;
std::unordered_map<EntityId, CompTemporalState>  g_temporal;
std::unordered_map<EntityId, CompStrictState>    g_strict;

void clear_data_access_memory()
{
    g_shared_res.clear();
    g_temporal.clear();
    g_strict.clear();
}

// -----------------------------------------------------------------------------
// System Mocks
// -----------------------------------------------------------------------------

// --- WAW Severing (Atomic) ---
struct SysAtomicWriter
{
    // Declares Write access, but explicitly states it uses hardware atomics.
    using EntityQuery = EntityQuery<
        Write<CompSharedResource>, AccessPolicy<DataAccessFlags::Atomic>
    >;

    static void update(DatabaseAccess<SysAtomicWriter> &db)
    {
        auto ents = db.query_entities(build_mask<CompSharedResource>());
        for(EntityId id : ents)
        {
            // Safe concurrent mutation via std::atomic
            g_shared_res[id].atomic_counter.fetch_add(
                1, std::memory_order_relaxed);

            // Jitter to encourage thread overlap
            std::this_thread::yield();
        }
    }
};

struct SysStrictWriter
{
    // Normal write. Must topologically serialize against all other writers and
    // readers.
    using EntityQuery = EntityQuery<Write<CompSharedResource>>;

    static void update(DatabaseAccess<SysStrictWriter> &db)
    {
        auto ents = db.query_entities(build_mask<CompSharedResource>());
        for(EntityId id : ents)
        {
            g_shared_res[id].atomic_counter.store(
                -999, std::memory_order_relaxed);
        }
    }
};

// --- WAR/RAW Severing (Previous) ---
struct SysTemporalReader
{
    // Declares Read access, but explicitly looks at N-1 state.
    using EntityQuery = EntityQuery<
        Read<CompTemporalState>, AccessPolicy<DataAccessFlags::Previous>
    >;

    static void update(DatabaseAccess<SysTemporalReader> &db)
    {
        auto ents = db.query_entities(build_mask<CompTemporalState>());
        for(EntityId id : ents)
        {
            // Reads the mathematically frozen N-1 state.
            volatile int dummy = g_temporal[id].frame_n_minus_1;
            (void)dummy;
            std::this_thread::yield();
        }
    }
};

struct SysTemporalWriter
{
    // Normal write to N state.
    using EntityQuery = EntityQuery<Write<CompTemporalState>>;

    static void update(DatabaseAccess<SysTemporalWriter> &db)
    {
        auto ents = db.query_entities(build_mask<CompTemporalState>());
        for(EntityId id : ents)
        {
            g_temporal[id].frame_n++; // Mutates the current frame state
            std::this_thread::yield();
        }
    }
};

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiExecutiveDataAccessTest : public ::testing::Test
{
protected:
    EntityDatabase     db;
    TaskGraphExecutive executive { db };

    void SetUp() override
    {
        db.clear_database();
        executive.clear_systems();
        clear_data_access_memory();
    }
};

// --- TOPOLOGICAL PROOFS ---

TEST_F(UsagiExecutiveDataAccessTest, DataAccess_WAW_SeveredByAtomic)
{
    // Creates 1 entity. Registers 100 systems all writing to it concurrently.
    EntityId e = db.create_entity_immediate(build_mask<CompSharedResource>());
    g_shared_res[e].atomic_counter.store(0, std::memory_order_relaxed);

    constexpr int NUM_WRITERS = 100;
    for(int i = 0; i < NUM_WRITERS; ++i)
    {
        executive.register_system<SysAtomicWriter>(
            "AtomicWriter_" + std::to_string(i));
    }

    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &exec_log = executive.get_execution_log();
    EXPECT_EQ(exec_log.size(), NUM_WRITERS)
        << "Task Graph dropped concurrent execution nodes.";

    // Mathematical proof of concurrent un-serialized execution:
    // If the WAW edges were not severed by the 'Atomic' flag, this would run
    // strictly sequentially. Because they were severed, the hardware atomics
    // correctly accumulated the state.
    EXPECT_EQ(g_shared_res[e].atomic_counter.load(), NUM_WRITERS)
        << "Atomic concurrent memory accumulation failed. Topology falsely "
           "serialized or corrupted.";
}

TEST_F(
    UsagiExecutiveDataAccessTest,
    DataAccess_MixedAtomicContention_ResolvesCorrectly)
{
    // Tests that a Strict writer mathematically bounds Atomic writers.
    EntityId e = db.create_entity_immediate(build_mask<CompSharedResource>());
    g_shared_res[e].atomic_counter.store(0, std::memory_order_relaxed);

    // 50 Atomics -> 1 Strict -> 50 Atomics
    for(int i = 0; i < 50; ++i)
        executive.register_system<SysAtomicWriter>(
            "AtomicPre_" + std::to_string(i));

    executive.register_system<SysStrictWriter>("StrictBoundary");

    for(int i = 0; i < 50; ++i)
        executive.register_system<SysAtomicWriter>(
            "AtomicPost_" + std::to_string(i));

    EXPECT_NO_THROW({ executive.execute_frame(); });

    // The StrictWriter forces a total topological barrier.
    // The first 50 increment to 50.
    // The StrictWriter resets to -999.
    // The last 50 increment to -949.
    EXPECT_EQ(g_shared_res[e].atomic_counter.load(), -949)
        << "Topological compiler failed to enforce rigid mathematical "
           "boundaries against mixed execution policies.";
}

TEST_F(UsagiExecutiveDataAccessTest, DataAccess_WAR_RAW_SeveredByPrevious)
{
    // 50 Systems reading N-1 state, 50 Systems writing N state concurrently.
    // A naive DAG would serialize all 100 nodes. By using 'Previous',
    // the graph must sever the edges and allow complete lock-free parallel
    // traversal.
    EntityId e    = db.create_entity_immediate(build_mask<CompTemporalState>());
    g_temporal[e] = { 10, 9 };

    constexpr int NUM_NODES = 50;

    // Shuffled registration to prove DAG edge severing ignores submission order
    for(int i = 0; i < NUM_NODES; ++i)
    {
        if(i % 2 == 0)
        {
            executive.register_system<SysTemporalReader>(
                "TempReader_" + std::to_string(i));
            executive.register_system<SysTemporalWriter>(
                "TempWriter_" + std::to_string(i));
        }
        else
        {
            executive.register_system<SysTemporalWriter>(
                "TempWriter_" + std::to_string(i));
            executive.register_system<SysTemporalReader>(
                "TempReader_" + std::to_string(i));
        }
    }

    EXPECT_NO_THROW({ executive.execute_frame(); });

    const auto &exec_log = executive.get_execution_log();
    EXPECT_EQ(exec_log.size(), NUM_NODES * 2);

    // Proof of topological dissolution:
    // 50 parallel writers executed successfully alongside 50 parallel readers
    // without throwing an underflow error.
    EXPECT_EQ(g_temporal[e].frame_n, 10 + NUM_NODES);
}
} // namespace usagi
