#include <atomic>
#include <random>
#include <thread>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <Usagi/Runtime/Allocators/BitmapAllocator.hpp>
#include <Usagi/Runtime/Storage/Backends/PagefileBackend.hpp>

using namespace usagi;
using namespace usagi::runtime;
using namespace usagi::runtime::allocators;

BOOST_AUTO_TEST_SUITE(BitmapAllocator_Tests)

BOOST_AUTO_TEST_CASE(Basic_Allocation_Deallocation)
{
    auto backend = storage::PagefileBackend::create(
        to_bytes(storage::StoragePageSize::_16MiB))
                       .value();
    auto view = backend.create_view().value();

    BitmapAllocator allocator(
        std::move(view), 256, storage::StorageAlignment::_64Byte, 10'000);

    auto h1 = allocator.allocate();
    BOOST_TEST(h1.is_valid());
    BOOST_TEST(h1.size == 256);

    auto h2 = allocator.allocate();
    BOOST_TEST(h2.is_valid());

    allocator.deallocate(h1);
    allocator.deallocate(h2);
}

BOOST_AUTO_TEST_CASE(Brutal_Multithreaded_Fuzzer)
{
    auto backend = storage::PagefileBackend::create(1'024 * 1'024 * 64).value();
    auto view    = backend.create_view().value();

    // Lock-free bitmap allocator. Test high concurrency with external
    // synchronization.
    BitmapAllocator allocator(
        std::move(view), 128, storage::StorageAlignment::_128Byte, 16 * 5'000);

    constexpr int NUM_THREADS       = 16;
    constexpr int ALLOCS_PER_THREAD = 5'000;

    std::vector<std::thread> threads;
    std::atomic<bool>        start_flag { false };
    std::atomic<int>         failed_allocs { 0 };

    // External synchronization for the allocator
    std::atomic_flag allocator_lock = ATOMIC_FLAG_INIT;

    // Vector to collect allocated handles so we can verify them later and
    // deallocate them
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
        "Failed to allocate blocks under high contention!");

    // Verify uniqueness
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
        "Duplicate handles allocated! External lock-free logic failed!");

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
