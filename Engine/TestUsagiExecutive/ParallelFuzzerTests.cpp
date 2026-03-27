/*
 * Usagi Engine: Parallel Topology Fuzzing & Race Condition Proofs
 * -----------------------------------------------------------------------------
 * Brutally stresses the Task Graph Executive utilizing maximum hardware
 * concurrency. Randomly shuffles DAG registration, injects chaotic JIT locks,
 * and intentionally crashes nodes. Data races are strictly monitored via
 * std::atomic MemoryChunkGuards.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <unordered_map>

#include <gtest/gtest.h>

#include "Executive.hpp"

namespace usagi
{
// -----------------------------------------------------------------------------
// Fuzzer Entropy Domain: Components & Bit Mappings
// -----------------------------------------------------------------------------
struct FuzzC00
{
    float data;
};

template <>
consteval ComponentMask get_component_bit<FuzzC00>()
{
    return 1ull << 20;
}

struct FuzzC01
{
    float data;
};

template <>
consteval ComponentMask get_component_bit<FuzzC01>()
{
    return 1ull << 21;
}

struct FuzzC02
{
    float data;
};

template <>
consteval ComponentMask get_component_bit<FuzzC02>()
{
    return 1ull << 22;
}

struct FuzzC03
{
    float data;
};

template <>
consteval ComponentMask get_component_bit<FuzzC03>()
{
    return 1ull << 23;
}

struct FuzzC04
{
    float data;
};

template <>
consteval ComponentMask get_component_bit<FuzzC04>()
{
    return 1ull << 24;
}

struct FuzzC05
{
    float data;
};

template <>
consteval ComponentMask get_component_bit<FuzzC05>()
{
    return 1ull << 25;
}

struct FuzzC06
{
    float data;
};

template <>
consteval ComponentMask get_component_bit<FuzzC06>()
{
    return 1ull << 26;
}

struct FuzzC07
{
    float data;
};

template <>
consteval ComponentMask get_component_bit<FuzzC07>()
{
    return 1ull << 27;
}

constexpr size_t NUM_FUZZ_COMPS = 8;
constexpr size_t FUZZ_BASE_BIT  = 20;

// -----------------------------------------------------------------------------
// Real-Time ThreadSanitizer: Atomic Memory Guards
// -----------------------------------------------------------------------------
/* Shio: This tracks active readers and writers for every component type.
   If the Task Graph DAG is flawed, threads will collide here and trigger a DATA
   RACE. */
struct MemoryChunkGuard
{
    std::atomic<int> readers { 0 };
    std::atomic<int> writers { 0 };

    void lock_read()
    {
        readers.fetch_add(1, std::memory_order_acquire);
        if(writers.load(std::memory_order_acquire) > 0)
        {
            throw std::runtime_error(
                "DATA RACE: Read overlapping with active Write!");
        }
    }

    void unlock_read() { readers.fetch_sub(1, std::memory_order_release); }

    void lock_write()
    {
        writers.fetch_add(1, std::memory_order_acquire);
        if(writers.load(std::memory_order_acquire) > 1)
        {
            throw std::runtime_error("DATA RACE: Multiple overlapping Writes!");
        }
        if(readers.load(std::memory_order_acquire) > 0)
        {
            throw std::runtime_error(
                "DATA RACE: Write overlapping with active Read!");
        }
    }

    void unlock_write() { writers.fetch_sub(1, std::memory_order_release); }
};

MemoryChunkGuard g_fuzz_guards[NUM_FUZZ_COMPS];

// Simulates workload and validates locks.
void execute_fuzz_payload(
    ComponentMask r_mask, ComponentMask w_mask, bool force_crash = false)
{
    std::vector<int> r_idx, w_idx;

    for(size_t i = 0; i < NUM_FUZZ_COMPS; ++i)
    {
        ComponentMask bit = 1ull << (FUZZ_BASE_BIT + i);
        if(w_mask & bit)
            w_idx.push_back(i);
        else if(r_mask & bit)
            r_idx.push_back(i);
    }

    // 1. Acquire mathematical locks. If DAG is broken, this throws.
    for(int i : w_idx)
        g_fuzz_guards[i].lock_write();
    for(int i : r_idx)
        g_fuzz_guards[i].lock_read();

    // 2. Brutalization: Artificial thread jitter to destroy sequential luck
    std::this_thread::yield();
    volatile double math_sink = 0.0;
    for(int i = 0; i < 2'000; ++i)
        math_sink += 1.0;

    // 3. Optional Chaotic Crash
    if(force_crash)
    {
        for(int i : r_idx)
            g_fuzz_guards[i].unlock_read();
        for(int i : w_idx)
            g_fuzz_guards[i].unlock_write();
        throw std::runtime_error("Fuzzer Induced Chaos Crash");
    }

    // 4. Release locks
    for(int i : r_idx)
        g_fuzz_guards[i].unlock_read();
    for(int i : w_idx)
        g_fuzz_guards[i].unlock_write();
}

// -----------------------------------------------------------------------------
// Randomized System Mocks
// -----------------------------------------------------------------------------
struct FuzzSys0
{
    using EntityQuery = EntityQuery<Write<FuzzC00>>;

    static void update(DatabaseAccess<FuzzSys0> &)
    {
        execute_fuzz_payload(0, get_component_bit<FuzzC00>());
    }
};

struct FuzzSys1
{
    using EntityQuery = EntityQuery<Read<FuzzC00>, Write<FuzzC01>>;

    static void update(DatabaseAccess<FuzzSys1> &)
    {
        execute_fuzz_payload(
            get_component_bit<FuzzC00>(), get_component_bit<FuzzC01>());
    }
};

struct FuzzSys2
{
    using EntityQuery = EntityQuery<Read<FuzzC00>, Write<FuzzC02>>;

    static void update(DatabaseAccess<FuzzSys2> &)
    {
        execute_fuzz_payload(
            get_component_bit<FuzzC00>(), get_component_bit<FuzzC02>());
    }
};

struct FuzzSys3
{
    using EntityQuery = EntityQuery<Write<FuzzC01, FuzzC02>>;

    static void update(DatabaseAccess<FuzzSys3> &)
    {
        execute_fuzz_payload(
            0, get_component_bit<FuzzC01>() | get_component_bit<FuzzC02>());
    }
};

struct FuzzSys4
{
    using EntityQuery = EntityQuery<Read<FuzzC01, FuzzC02>, Write<FuzzC03>>;

    static void update(DatabaseAccess<FuzzSys4> &)
    {
        execute_fuzz_payload(
            get_component_bit<FuzzC01>() | get_component_bit<FuzzC02>(),
            get_component_bit<FuzzC03>());
    }
};

struct FuzzSys5
{
    using EntityQuery = EntityQuery<Read<FuzzC03>, Write<FuzzC04, FuzzC05>>;

    static void update(DatabaseAccess<FuzzSys5> &)
    {
        execute_fuzz_payload(
            get_component_bit<FuzzC03>(),
            get_component_bit<FuzzC04>() | get_component_bit<FuzzC05>());
    }
};

struct FuzzSys6
{
    using EntityQuery = EntityQuery<Read<FuzzC04, FuzzC05>, Write<FuzzC06>>;

    static void update(DatabaseAccess<FuzzSys6> &)
    {
        execute_fuzz_payload(
            get_component_bit<FuzzC04>() | get_component_bit<FuzzC05>(),
            get_component_bit<FuzzC06>());
    }
};

/* Yukino: JIT Escalator. Dynamically expands its write lock based on the
 * database. */
struct FuzzSysJITDelete
{
    using EntityQuery = EntityQuery<Read<FuzzC07>, IntentDelete<FuzzC07>>;

    static void update(DatabaseAccess<FuzzSysJITDelete> &db)
    {
        // Simulating the base read/write locking
        execute_fuzz_payload(
            get_component_bit<FuzzC07>(), get_component_bit<FuzzC07>());

        auto ents = db.query_entities(get_component_bit<FuzzC07>());
        for(auto id : ents)
            db.destroy_entity(id);
    }
};

/* Shio: Chaos Node. Intentionally crashes to test topological abortion
 * propagation across threads. */
struct FuzzSysChaosCrash
{
    using EntityQuery = EntityQuery<Write<FuzzC06>>;

    static void update(DatabaseAccess<FuzzSysChaosCrash> &)
    {
        execute_fuzz_payload(0, get_component_bit<FuzzC06>(), true);
    }
};

// -----------------------------------------------------------------------------
// Google Test Suite
// -----------------------------------------------------------------------------
class UsagiExecutiveParallelFuzzer : public ::testing::Test
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

// --- MULTITHREADED FUZZING PROOFS ---

TEST_F(UsagiExecutiveParallelFuzzer, Fuzz_RandomizedDAG_ParallelDataRaces)
{
    unsigned int hw_concurrency = std::thread::hardware_concurrency();
    if(hw_concurrency == 0) hw_concurrency = 4;
    std::cout << "[ INFO     ] Unleashing fuzzer across " << hw_concurrency
              << " hardware threads." << std::endl;

    std::mt19937 rng(1'337); // Fixed seed for reproducible topological faults

    std::vector<std::function<void()>> system_generators = {
        [&]() { executive.register_system<FuzzSys0>("Sys0"); },
        [&]() { executive.register_system<FuzzSys1>("Sys1"); },
        [&]() { executive.register_system<FuzzSys2>("Sys2"); },
        [&]() { executive.register_system<FuzzSys3>("Sys3"); },
        [&]() { executive.register_system<FuzzSys4>("Sys4"); },
        [&]() { executive.register_system<FuzzSys5>("Sys5"); },
        [&]() { executive.register_system<FuzzSys6>("Sys6"); },
        [&]() { executive.register_system<FuzzSysJITDelete>("SysJITDelete"); }
    };

    int           data_races_detected = 0;
    constexpr int ITERATIONS = 200; // Brutalize the scheduler 200 times

    for(int i = 0; i < ITERATIONS; ++i)
    {
        db.clear_database();
        executive.clear_systems();

        // 1. Randomize Entity Matrix (Entropy generation)
        for(int e = 0; e < 20; ++e)
        {
            ComponentMask rand_mask = 0;
            for(int b = 0; b < NUM_FUZZ_COMPS; ++b)
            {
                if(rng() % 4 == 0) rand_mask |= (1ull << (FUZZ_BASE_BIT + b));
            }
            if(rand_mask != 0) db.create_entity_immediate(rand_mask);
        }

        // 2. Randomize DAG Registration Order (This fundamentally alters
        // happens-before edges)
        std::shuffle(system_generators.begin(), system_generators.end(), rng);
        int sys_to_register = 4 + (rng() % (system_generators.size() - 3));

        for(int s = 0; s < sys_to_register; ++s)
        {
            system_generators[s]();
        }

        // 3. Execute the multithreaded wavefront
        executive.execute_frame();

        // 4. Verify no atomic guard detected a race condition
        const auto &err_log = executive.get_error_log();
        for(const auto &err : err_log)
        {
            if(err.find("DATA RACE") != std::string::npos)
            {
                data_races_detected++;
                std::cerr << "TOPOLOGICAL FAILURE DETECTED: " << err
                          << std::endl;
            }
        }
    }

    EXPECT_EQ(data_races_detected, 0)
        << "The Task Graph Scheduler failed to prevent data races during "
           "high-entropy fuzzing.";
}

TEST_F(UsagiExecutiveParallelFuzzer, Fuzz_ChaosException_ThreadSafety)
{
    // Proves that when running asynchronously, an exception in one branch
    // does not deadlock the `std::condition_variable` or permanently lock the
    // thread pool.

    std::cout << "[ INFO     ] Initiating Chaos Exception Fuzzing."
              << std::endl;

    for(int i = 0; i < 50; ++i)
    {
        db.clear_database();
        executive.clear_systems();

        // Populate Database
        db.create_entity_immediate(get_component_bit<FuzzC00>());

        // Register standard graph with a random chaos node
        executive.register_system<FuzzSys0>("RootNode");
        executive.register_system<FuzzSys1>("SafeBranchA");
        executive.register_system<FuzzSys2>("SafeBranchB");
        executive.register_system<FuzzSysChaosCrash>("ChaosCrash");
        executive.register_system<FuzzSys6>(
            "DependentOnCrash"); // FuzzC06 requires ChaosCrash

        // The frame MUST complete. Deadlocks will hang the GTest suite
        // entirely.
        EXPECT_NO_THROW({ executive.execute_frame(); });

        // Verify the abortion cascade traversed the thread pool cleanly
        const auto &exec_log          = executive.get_execution_log();
        bool        root_ran          = false;
        bool        dependent_aborted = false;

        for(const auto &log : exec_log)
        {
            if(log == "Executed: RootNode") root_ran = true;
            if(log == "ABORTED: DependentOnCrash") dependent_aborted = true;
        }

        EXPECT_TRUE(root_ran) << "Safe thread branch failed to execute due to "
                                 "parallel exception.";
        EXPECT_TRUE(dependent_aborted) << "Executive failed to synchronize "
                                          "exception abortion across threads.";
    }
}
} // namespace usagi
