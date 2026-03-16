#include <atomic>
#include <thread>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <Usagi/Runtime/Allocators/LinearBumpAllocator.hpp>
#include <Usagi/Runtime/Storage/Backends/PagefileBackend.hpp>

using namespace usagi::runtime;
using namespace usagi::runtime::allocators;

BOOST_AUTO_TEST_SUITE(LinearBumpAllocator_Tests)

BOOST_AUTO_TEST_CASE(Basic_Allocation)
{
    auto backend = storage::PagefileBackend::create(1'024 * 1'024 * 64).value();
    auto view    = backend.create_view().value();

    LinearBumpAllocator allocator(std::move(view));

    auto h1 = allocator.allocate(128, storage::StorageAlignment::_16Byte);
    BOOST_TEST(h1.is_valid());
    BOOST_TEST(h1.size == 128);

    auto h2 = allocator.allocate(256, storage::StorageAlignment::_64Byte);
    BOOST_TEST(h2.is_valid());
    BOOST_CHECK(h2.offset % 64 == 0);

    allocator.reset();

    auto h3 = allocator.allocate(512, storage::StorageAlignment::_16Byte);
    BOOST_TEST(h3.is_valid());
    // Since it reset, it should reuse the first logical spot
    BOOST_CHECK_EQUAL(h3.offset, h1.offset);
}

BOOST_AUTO_TEST_CASE(Brutal_WaitFree_Multithreaded_Allocation)
{
    // The LinearBumpAllocator is designed to be wait-free and thread-safe.
    // We will launch 16 threads, each allocating 10,000 blocks of random sizes,
    // and verify that none of the memory regions overlap.

    auto backend =
        storage::PagefileBackend::create(1'024 * 1'024 * 256).value(); // 256MB
    auto view = backend.create_view().value();

    LinearBumpAllocator allocator(std::move(view));

    constexpr int NUM_THREADS       = 16;
    constexpr int ALLOCS_PER_THREAD = 10'000;

    std::vector<std::thread>               threads;
    std::vector<std::vector<MemoryHandle>> thread_handles(NUM_THREADS);

    std::atomic<bool> start_flag { false };

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
                // Jumble sizes and alignments
                std::uint64_t             size  = (j % 128) + 16;
                storage::StorageAlignment align = (j % 2 == 0)
                    ? storage::StorageAlignment::_16Byte
                    : storage::StorageAlignment::_64Byte;

                auto handle = allocator.allocate(size, align);
                thread_handles[i].push_back(handle);
            }
        });
    }

    start_flag.store(true, std::memory_order_release);

    for(auto &t : threads)
    {
        t.join();
    }

    // Verify no overlaps
    struct Span
    {
        uint64_t start;
        uint64_t end;
    };

    std::vector<Span> all_spans;
    all_spans.reserve(NUM_THREADS * ALLOCS_PER_THREAD);

    for(const auto &vec : thread_handles)
    {
        for(const auto &h : vec)
        {
            BOOST_TEST(h.is_valid());
            all_spans.push_back({ h.offset, h.offset + h.size });
        }
    }

    std::ranges::sort(all_spans, [](const Span &a, const Span &b) {
        return a.start < b.start;
    });

    for(size_t i = 0; i < all_spans.size() - 1; ++i)
    {
        BOOST_TEST(
            all_spans[i].end <= all_spans[i + 1].start,
            "Memory regions overlapped! Wait-free CAS failed!");
    }
}

BOOST_AUTO_TEST_SUITE_END()
