#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <Usagi/Runtime/Allocators/HiveAllocator.hpp>
#include <Usagi/Runtime/Storage/Backends/PagefileBackend.hpp>

using namespace usagi;
using namespace usagi::runtime;
using namespace usagi::runtime::allocators;

BOOST_AUTO_TEST_SUITE(HiveAllocator_Tests)

BOOST_AUTO_TEST_CASE(Basic_Allocation_Deallocation)
{
    auto backend = storage::PagefileBackend::create(1'024 * 1'024 * 64).value();
    auto view    = backend.create_view().value();

    HiveAllocator allocator(std::move(view), OperandBitWidth::_64Bit);
    allocator.initialize(128, 64); // 128 byte chunks, aligned to 64 bytes

    // It should pack exactly 64 entities per page (using a uint64_t mask)
    std::vector<MemoryHandle> handles;
    for(int i = 0; i < 100; ++i)
    {
        handles.push_back(allocator.allocate());
    }

    for(int i = 0; i < 100; ++i)
    {
        BOOST_TEST(handles[i].is_valid());
        // Handle logic packs Page ID in high 32 bits, local index in low 32
        // bits.
        uint32_t page_id = handles[i].offset >> 32;
        uint32_t slot_id = handles[i].offset & 0xFFFF'FFFF;

        // Items 0-63 go to page 1. (Page 0 is the HeapHeader)
        if(i < 64)
        {
            BOOST_TEST(page_id == 1);
            BOOST_TEST(slot_id == i);
        }
        else
        {
            BOOST_TEST(page_id == 2);
            BOOST_TEST(slot_id == (i - 64));
        }
    }

    // Deallocate half of them
    for(int i = 0; i < 50; ++i)
    {
        allocator.deallocate(handles[i]);
    }

    // Reallocate, they should fill the holes in Page 1
    for(int i = 0; i < 50; ++i)
    {
        auto     h       = allocator.allocate();
        uint32_t page_id = h.offset >> 32;
        BOOST_TEST(page_id == 1);
    }
}

BOOST_AUTO_TEST_CASE(Brutal_Fragmentation_Fuzzer)
{
    auto backend = storage::PagefileBackend::create(1'024 * 1'024 * 64).value();
    auto view    = backend.create_view().value();

    HiveAllocator allocator(std::move(view), OperandBitWidth::_64Bit);
    allocator.initialize(256, 16);

    std::mt19937 gen(42);

    std::vector<MemoryHandle> active_allocs;
    active_allocs.reserve(20'000);

    // 1. Allocate heavily to create many pages.
    for(int i = 0; i < 15'000; ++i)
    {
        auto h = allocator.allocate();
        if(h.is_valid())
        {
            active_allocs.push_back(h);
        }
    }

    // 2. Randomly deallocate 75% of them to cause extreme fragmentation and
    // page linking/unlinking
    std::shuffle(active_allocs.begin(), active_allocs.end(), gen);
    size_t to_remove = static_cast<size_t>(active_allocs.size() * 0.75);
    for(size_t i = 0; i < to_remove; ++i)
    {
        allocator.deallocate(active_allocs[i]);
    }
    active_allocs.erase(
        active_allocs.begin(), active_allocs.begin() + to_remove);

    // 3. Allocate back up to verify that free pages were properly linked in the
    // chain
    for(int i = 0; i < 15'000; ++i)
    {
        auto h = allocator.allocate();
        if(h.is_valid())
        {
            active_allocs.push_back(h);
        }
    }

    // 4. Deallocate everything
    for(auto h : active_allocs)
    {
        allocator.deallocate(h);
    }

    // Test logic correctness on resolving data pointer.
    auto final_h = allocator.allocate();
    BOOST_TEST(final_h.is_valid());
    BOOST_TEST(allocator.resolve(final_h) != nullptr);
}

BOOST_AUTO_TEST_CASE(Brutal_Multithreaded_Fuzzer)
{
    auto backend =
        storage::PagefileBackend::create(1'024 * 1'024 * 128).value();
    auto view = backend.create_view().value();

    HiveAllocator allocator(std::move(view), OperandBitWidth::_64Bit);
    allocator.initialize(64, 8);

    constexpr int NUM_THREADS       = 16;
    constexpr int ALLOCS_PER_THREAD = 10'000;

    std::vector<std::thread> threads;
    std::atomic<bool>        start_flag { false };
    std::atomic<int>         failed_allocs { 0 };

    // External synchronization for the allocator since HiveAllocator is not
    // inherently thread-safe
    std::atomic_flag allocator_lock = ATOMIC_FLAG_INIT;

    std::vector<std::vector<MemoryHandle>> thread_handles(NUM_THREADS);

    for(int i = 0; i < NUM_THREADS; ++i)
    {
        thread_handles[i].reserve(ALLOCS_PER_THREAD);
        threads.emplace_back([&, i]() {
            while(!start_flag.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for(int j = 0; j < ALLOCS_PER_THREAD; ++j)
            {
                while(allocator_lock.test_and_set(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                auto h = allocator.allocate();
                allocator_lock.clear(std::memory_order_release);

                if(h.is_valid())
                {
                    thread_handles[i].push_back(h);
                }
                else
                {
                    failed_allocs.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    start_flag.store(true, std::memory_order_release);

    for(auto &t : threads)
    {
        t.join();
    }

    BOOST_TEST(
        failed_allocs.load() == 0,
        "Failed to allocate pages under high contention!");

    // Verify uniqueness of handles
    std::vector<uint64_t> offsets;
    offsets.reserve(NUM_THREADS * ALLOCS_PER_THREAD);
    for(const auto &vec : thread_handles)
    {
        for(const auto &h : vec)
        {
            offsets.push_back(h.offset);
        }
    }

    std::ranges::sort(offsets);
    auto it = std::ranges::unique(offsets).begin();
    BOOST_CHECK_MESSAGE(
        it == offsets.end(),
        "Duplicate physical offsets allocated! Hive logic failed!");

    // Deallocate everything
    for(const auto &vec : thread_handles)
    {
        for(const auto &h : vec)
        {
            while(allocator_lock.test_and_set(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            allocator.deallocate(h);
            allocator_lock.clear(std::memory_order_release);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
