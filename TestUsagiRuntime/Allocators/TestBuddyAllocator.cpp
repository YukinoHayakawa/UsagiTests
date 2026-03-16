#include <random>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <Usagi/Runtime/Allocators/BuddyAllocator.hpp>
#include <Usagi/Runtime/Storage/Backends/PagefileBackend.hpp>

using namespace usagi::runtime;
using namespace usagi::runtime::allocators;

BOOST_AUTO_TEST_SUITE(BuddyAllocator_Tests)

BOOST_AUTO_TEST_CASE(Basic_Allocation_Deallocation)
{
    // 64MB Backend
    auto backend = storage::PagefileBackend::create(1'024 * 1'024 * 64).value();
    auto view    = backend.create_view().value();

    // We map a buddy allocator
    BuddyAllocator allocator(std::move(view), 4'096, 1'024 * 1'024 * 64);

    auto h1 = allocator.allocate(100, storage::StorageAlignment::_16Byte);
    BOOST_TEST(h1.is_valid());
    BOOST_TEST(h1.size == 100);

    auto h2 = allocator.allocate(5'000, storage::StorageAlignment::_16Byte);
    BOOST_TEST(h2.is_valid());

    allocator.deallocate(h1);
    allocator.deallocate(h2);
}

BOOST_AUTO_TEST_CASE(Brutal_Fragmentation_Fuzzer)
{
    // Fuzzing the buddy allocator to ensure no memory leaks and proper
    // coalescing
    auto backend = storage::PagefileBackend::create(1'024 * 1'024 * 32).value();
    auto view    = backend.create_view().value();

    BuddyAllocator allocator(std::move(view), 4'096, 1'024 * 1'024 * 32);

    std::mt19937                                 gen(42);
    std::uniform_int_distribution<std::uint64_t> dist_size(
        16, 1'024 * 128); // 16B to 128KB chunks

    std::vector<MemoryHandle> active_allocs;
    active_allocs.reserve(10'000);

    // 1. Allocate a ton of memory
    for(int i = 0; i < 5'000; ++i)
    {
        auto h = allocator.allocate(
            dist_size(gen), storage::StorageAlignment::_64Byte);
        if(h.is_valid())
        {
            active_allocs.push_back(h);
        }
    }

    // 2. Randomly deallocate half of them
    std::shuffle(active_allocs.begin(), active_allocs.end(), gen);
    size_t half = active_allocs.size() / 2;
    for(size_t i = 0; i < half; ++i)
    {
        allocator.deallocate(active_allocs[i]);
    }
    active_allocs.erase(active_allocs.begin(), active_allocs.begin() + half);

    // 3. Allocate again to force fragmentation traversal
    for(int i = 0; i < 5'000; ++i)
    {
        auto h = allocator.allocate(
            dist_size(gen), storage::StorageAlignment::_64Byte);
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

    // 5. Ensure the entire memory block is fully coalesced back into the
    // largest chunk We do this by attempting to allocate almost the entire
    // capacity minus headers.
    auto massive_handle = allocator.allocate(
        1'024 * 1'024 * 31, storage::StorageAlignment::_4KiB);
    BOOST_TEST(
        massive_handle.is_valid(),
        "Failed to allocate massive block. Buddy blocks did not properly "
        "coalesce!");
}

BOOST_AUTO_TEST_SUITE_END()
