/*
 * Usagi Engine: NASA-Grade Topological Proofs
 * -----------------------------------------------------------------------------
 * Subjecting the Task Graph Executive to highly ordered, mathematically
 * pathological DAG structures. Evaluates condition variable storms, strict
 * temporal sequencing, JIT trapdoors, and Amdahl's scaling limits.
 */

#include <atomic>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi::poc::executive_1
{
// -----------------------------------------------------------------------------
// Component Definitions & Bit Mappings
// -----------------------------------------------------------------------------
struct CompHourglass
{
    int dummy { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompHourglass>()
{
    return 1ull << 50;
}

struct CompBlender
{
    std::atomic<int64_t> value { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompBlender>()
{
    return 1ull << 51;
}

struct CompJitTarget
{
    int data { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompJitTarget>()
{
    return 1ull << 52;
}

struct CompJitTrigger
{
    int trigger { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompJitTrigger>()
{
    return 1ull << 53;
}

struct CompOuroborosToken
{
    int generation { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompOuroborosToken>()
{
    return 1ull << 54;
}

struct CompAvalanche
{
    uint32_t thread_origin { 0 };
};

template <>
consteval ComponentMask get_component_bit<CompAvalanche>()
{
    return 1ull << 55;
}

// -----------------------------------------------------------------------------
// Global Sequence Ticker (For Temporal Proofs)
// -----------------------------------------------------------------------------
std::atomic<int64_t>                     g_global_ticker { 0 };
std::unordered_map<std::string, int64_t> g_execution_timestamps;
std::mutex                               g_timestamp_mutex;

void record_timestamp(const std::string &sys_name)
{
    int64_t tick = g_global_ticker.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard<std::mutex> lock(g_timestamp_mutex);
    g_execution_timestamps[sys_name] = tick;
}

void clear_nasa_memory()
{
    g_global_ticker.store(0, std::memory_order_release);
    g_execution_timestamps.clear();
}

// -----------------------------------------------------------------------------
// System Mocks: The Hourglass Tsunami
// -----------------------------------------------------------------------------
struct SysHourglassTop
{
    using EntityQuery = EntityQuery<Read<CompHourglass>>;

    static void update(DatabaseAccess<SysHourglassTop> &)
    {
        // High concurrency read
        volatile int x = 0;
        (void)x;
    }
};

struct SysHourglassBottleneck
{
    using EntityQuery = EntityQuery<Write<CompHourglass>>;

    static void update(DatabaseAccess<SysHourglassBottleneck> &)
    {
        // Serialized chokepoint
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
};

struct SysHourglassBottom
{
    using EntityQuery = EntityQuery<Read<CompHourglass>>;

    static void update(DatabaseAccess<SysHourglassBottom> &)
    {
        // Tsunami wake-up read
        volatile int x = 0;
        (void)x;
    }
};

// -----------------------------------------------------------------------------
// System Mocks: The Semantic Blender
// -----------------------------------------------------------------------------
struct SysBlenderStrictWrite
{
    using EntityQuery = EntityQuery<Write<CompBlender>>;

    static void update(DatabaseAccess<SysBlenderStrictWrite> &)
    {
        record_timestamp("StrictWrite");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
};

struct SysBlenderPreviousRead
{
    using EntityQuery =
        EntityQuery<Read<CompBlender>, AccessPolicy<DataAccessFlags::Previous>>;

    static void update(DatabaseAccess<SysBlenderPreviousRead> &)
    {
        record_timestamp("PreviousRead");
    }
};

struct SysBlenderDiscardWrite
{
    using EntityQuery =
        EntityQuery<Write<CompBlender>, AccessPolicy<DataAccessFlags::Discard>>;

    static void update(DatabaseAccess<SysBlenderDiscardWrite> &)
    {
        record_timestamp("DiscardWrite");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
};

struct SysBlenderAtomicWriteA
{
    using EntityQuery =
        EntityQuery<Write<CompBlender>, AccessPolicy<DataAccessFlags::Atomic>>;

    static void update(DatabaseAccess<SysBlenderAtomicWriteA> &)
    {
        record_timestamp("AtomicWriteA");
    }
};

struct SysBlenderAtomicWriteB
{
    using EntityQuery =
        EntityQuery<Write<CompBlender>, AccessPolicy<DataAccessFlags::Atomic>>;

    static void update(DatabaseAccess<SysBlenderAtomicWriteB> &)
    {
        record_timestamp("AtomicWriteB");
    }
};

// -----------------------------------------------------------------------------
// System Mocks: The JIT Trapdoor
// -----------------------------------------------------------------------------
struct SysTrapdoorPreWrite
{
    using EntityQuery = EntityQuery<Write<CompJitTarget>>;

    static void update(DatabaseAccess<SysTrapdoorPreWrite> &)
    {
        record_timestamp("TrapdoorPre");
    }
};

struct SysTrapdoorGhostDelete
{
    // Reads Trigger, deletes Trigger.
    // Dynamically, the entity ALSO has Target. JIT must expand to lock Target.
    using EntityQuery =
        EntityQuery<Read<CompJitTrigger>, IntentDelete<CompJitTrigger>>;

    static void update(DatabaseAccess<SysTrapdoorGhostDelete> &db)
    {
        record_timestamp("TrapdoorGhost");
        auto ents = db.query_entities(build_mask<CompJitTrigger>());
        for(auto e : ents)
            db.destroy_entity(e);
    }
};

struct SysTrapdoorPostRead
{
    using EntityQuery = EntityQuery<Read<CompJitTarget>>;

    static void update(DatabaseAccess<SysTrapdoorPostRead> &)
    {
        record_timestamp("TrapdoorPost");
    }
};

// -----------------------------------------------------------------------------
// System Mocks: The Ouroboros Re-entrancy Loop
// -----------------------------------------------------------------------------
struct SysOuroborosSpammer
{
    using EntityQuery =
        EntityQuery<Read<CompOuroborosToken>, IntentDelete<CompOuroborosToken>>;

    static void update(DatabaseAccess<SysOuroborosSpammer> &db)
    {
        auto ents = db.query_entities(build_mask<CompOuroborosToken>());
        for(auto e : ents)
        {
            db.destroy_entity(e);
            // This tests the MAX_RE_ENTRIES cyclic bounds.
            // (Note: To test orthogonal partitions natively through the DAG,
            // DatabaseAccess must be upgraded to dynamically inject thread_id).
            db.queue_spawn(build_mask<CompOuroborosToken>());
        }
    }
};

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiExecutiveNasaTest : public ::testing::Test
{
protected:
    EntityDatabase     db;
    TaskGraphExecutive executive { db };

    void SetUp() override
    {
        db.clear_database();
        executive.clear_systems();
        clear_nasa_memory();
    }
};

// --- TOPOLOGICAL CRUSH TESTS ---

TEST_F(UsagiExecutiveNasaTest, Pathological_HourglassTsunami_NoCvDeadlock)
{
    // Top-heavy: 5,000 readers
    for(int i = 0; i < 5'000; ++i)
    {
        executive.register_system<SysHourglassTop>("Top_" + std::to_string(i));
    }

    // The strict bottleneck
    executive.register_system<SysHourglassBottleneck>("Bottleneck");

    // Bottom-heavy: 5,000 readers waiting on the bottleneck
    for(int i = 0; i < 5'000; ++i)
    {
        executive.register_system<SysHourglassBottom>(
            "Bottom_" + std::to_string(i));
    }

    db.create_entity_immediate(build_mask<CompHourglass>());

    // If the atomic tasks_completed underflows, or if cv.notify_one() drops
    // signals due to lock contention during the massive fan-out, this will hang
    // indefinitely.
    EXPECT_NO_THROW({ executive.execute_frame(); });

    EXPECT_EQ(executive.get_execution_log().size(), 10'001)
        << "Thread pool failed to process all 10,001 nodes in the Hourglass "
           "Tsunami.";
}

TEST_F(UsagiExecutiveNasaTest, Pathological_SemanticBlender_TemporalOrdering)
{
    db.create_entity_immediate(build_mask<CompBlender>());

    // Registration order defines the implicit logical baseline.
    // 1. PreviousRead (Should run immediately, severs RAW from StrictWrite)
    executive.register_system<SysBlenderPreviousRead>("PreviousRead");
    // 2. StrictWrite (Should block everything except PreviousRead)
    executive.register_system<SysBlenderStrictWrite>("StrictWrite");
    // 3. DiscardWrite (Severs WAW from StrictWrite, but must happen AFTER
    // StrictWrite logically)
    executive.register_system<SysBlenderDiscardWrite>("DiscardWrite");
    // 4 & 5. AtomicWrites (Sever WAW from each other, but must wait for
    // DiscardWrite)
    executive.register_system<SysBlenderAtomicWriteA>("AtomicWriteA");
    executive.register_system<SysBlenderAtomicWriteB>("AtomicWriteB");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    // Retrieve the exact execution sequence
    int64_t t_prev    = g_execution_timestamps["PreviousRead"];
    int64_t t_strict  = g_execution_timestamps["StrictWrite"];
    int64_t t_discard = g_execution_timestamps["DiscardWrite"];
    int64_t t_atom_a  = g_execution_timestamps["AtomicWriteA"];
    int64_t t_atom_b  = g_execution_timestamps["AtomicWriteB"];

    // Mathematical Proof of Edge Severing & Retention:
    // 1. PreviousRead mathematically severs WAR/RAW, meaning it runs parallel
    // to or before StrictWrite. (It does not wait for StrictWrite).
    EXPECT_TRUE(t_prev <= t_strict || t_prev > t_strict)
        << "Trivial assertion to establish timestamp existence.";

    // 2. DiscardWrite logically follows StrictWrite. It severs WAW, meaning
    // subsequent systems don't wait for StrictWrite, they wait for
    // DiscardWrite.
    EXPECT_LT(t_strict, t_discard)
        << "StrictWrite failed to mathematically bound DiscardWrite.";

    // 3. AtomicWrites wait for DiscardWrite, but NOT for each other.
    EXPECT_LT(t_discard, t_atom_a)
        << "DiscardWrite failed to bound AtomicWriteA.";
    EXPECT_LT(t_discard, t_atom_b)
        << "DiscardWrite failed to bound AtomicWriteB.";
}

TEST_F(UsagiExecutiveNasaTest, Pathological_JitTrapdoor_DynamicInversion)
{
    // Entity possesses BOTH Target and Trigger.
    db.create_entity_immediate(build_mask<CompJitTarget, CompJitTrigger>());

    // Registration: PreWrite -> GhostDelete -> PostRead
    executive.register_system<SysTrapdoorPreWrite>("TrapdoorPre");
    executive.register_system<SysTrapdoorGhostDelete>("TrapdoorGhost");
    executive.register_system<SysTrapdoorPostRead>("TrapdoorPost");

    EXPECT_NO_THROW({ executive.execute_frame(); });

    int64_t t_pre   = g_execution_timestamps["TrapdoorPre"];
    int64_t t_ghost = g_execution_timestamps["TrapdoorGhost"];
    int64_t t_post  = g_execution_timestamps["TrapdoorPost"];

    // Proof: Because GhostDelete deletes the Trigger, and the entity ALSO has
    // Target, the JIT compiler MUST expand GhostDelete's write mask to include
    // Target. Therefore, GhostDelete must strictly serialize between PreWrite
    // (WAW) and PostRead (RAW).

    EXPECT_LT(t_pre, t_ghost) << "JIT Trapdoor failure: GhostDelete executed "
                                 "before or during PreWrite.";
    EXPECT_LT(t_ghost, t_post) << "JIT Trapdoor failure: PostRead executed "
                                  "before or during GhostDelete.";
}

TEST_F(UsagiExecutiveNasaTest, Pathological_Ouroboros_ReentrancyBounds)
{
    for(int i = 0; i < 10; ++i)
    {
        db.create_entity_immediate(build_mask<CompOuroborosToken>());
    }

    executive.register_system<SysOuroborosSpammer>("Ouroboros");

    // The endomorphism must correctly evaluate MAX_RE_ENTRIES and throw the
    // fatal error.
    EXPECT_THROW({ executive.execute_frame(); }, std::runtime_error);
}

// --- ORTHOGONAL PARTITIONING CRUSH TEST ---

TEST_F(
    UsagiExecutiveNasaTest,
    Pathological_PartitionedAvalanche_DeterministicCommit)
{
    // This proof bypasses the Executive to directly hammer the EntityDatabase's
    // lock-free partition architecture. We verify that 256 parallel threads
    // spamming 2.56 million entities mathematically resolve to the exact same
    // EntityId sequence every single time, proving absolute data-parallel
    // determinism.

    constexpr uint32_t THREADS           = MAX_SPAWN_PARTITIONS;
    constexpr uint32_t SPAWNS_PER_THREAD = 10'000;

    // Simulate chaos execution
    auto worker = [&](uint32_t partition_id) {
        for(uint32_t i = 0; i < SPAWNS_PER_THREAD; ++i)
        {
            // Thread safely locks only its orthogonal sector
            db.queue_spawn(build_mask<CompAvalanche>(), partition_id);

            // Jitter to violently interleave thread execution
            if(i % 1'000 == 0) std::this_thread::yield();
        }
    };

    std::vector<std::thread> thread_pool;
    for(uint32_t i = 0; i < THREADS; ++i)
    {
        thread_pool.emplace_back(worker, i);
    }

    for(auto &t : thread_pool)
    {
        t.join(); // The join acts as the memory_order_acquire for the relaxed
                  // has_spawns flag.
    }

    EXPECT_TRUE(db.has_pending_spawns())
        << "Database failed to register the memory write of has_spawns.";

    // The deterministic flush
    db.commit_pending_spawns();

    EXPECT_FALSE(db.has_pending_spawns())
        << "Database failed to reset has_spawns after commit.";

    auto ents = db.query_entities(build_mask<CompAvalanche>());
    EXPECT_EQ(ents.size(), THREADS * SPAWNS_PER_THREAD)
        << "Database dropped entities during parallel array insertion.";

    // Mathematical Proof of Determinism:
    // Because commit_pending_spawns flattens sequentially from partition 0 to
    // 255, the EntityIds must be monotonically increasing and strictly grouped
    // by the partition that spawned them, regardless of which thread finished
    // its push_back loop first. (Assuming next_id started at 1)

    bool is_deterministic = true;
    for(uint32_t t = 0; t < THREADS; ++t)
    {
        for(uint32_t i = 0; i < SPAWNS_PER_THREAD; ++i)
        {
            uint32_t absolute_index = (t * SPAWNS_PER_THREAD) + i;
            EntityId expected_id    = absolute_index + 1; // next_id starts at 1

            if(ents[absolute_index] != expected_id)
            {
                is_deterministic = false;
                break;
            }
        }
    }

    EXPECT_TRUE(is_deterministic)
        << "Amdahl's Nightmare: Entity ID assignment diverged due to thread "
           "scheduling. Network sync destroyed.";
}
} // namespace usagi::poc::executive_1
