#include <random>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <Usagi/Runtime/Allocators/TLSFAllocator.hpp>
#include <Usagi/Runtime/Storage/Backends/PagefileBackend.hpp>

using namespace usagi::runtime;
using namespace usagi::runtime::allocators;

BOOST_AUTO_TEST_SUITE(TLSFAllocator_Tests)

BOOST_AUTO_TEST_CASE(Basic_Allocation_Deallocation)
{
    auto backend = storage::PagefileBackend::create(1'024 * 1'024 * 64).value();
    auto view    = backend.create_view().value();

    TLSFAllocator allocator(std::move(view), 1'024 * 1'024 * 64);

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
    auto backend = storage::PagefileBackend::create(1'024 * 1'024 * 32).value();
    auto view    = backend.create_view().value();

    TLSFAllocator allocator(std::move(view), 1'024 * 1'024 * 32);

    std::mt19937                                 gen(42);
    // TLSF handles small churn incredibly well, so we hit it with very small to
    // medium allocations
    std::uniform_int_distribution<std::uint64_t> dist_size(4, 1'024 * 64);

    std::vector<MemoryHandle> active_allocs;
    active_allocs.reserve(10'000);

    for(int i = 0; i < 8'000; ++i)
    {
        auto h = allocator.allocate(
            dist_size(gen), storage::StorageAlignment::_16Byte);
        if(h.is_valid())
        {
            active_allocs.push_back(h);
        }
    }

    std::ranges::shuffle(active_allocs, gen);
    size_t half = active_allocs.size() / 2;
    for(size_t i = 0; i < half; ++i)
    {
        allocator.deallocate(active_allocs[i]);
    }
    active_allocs.erase(active_allocs.begin(), active_allocs.begin() + half);

    for(int i = 0; i < 8'000; ++i)
    {
        auto h = allocator.allocate(
            dist_size(gen), storage::StorageAlignment::_16Byte);
        if(h.is_valid())
        {
            active_allocs.push_back(h);
        }
    }

    for(auto h : active_allocs)
    {
        allocator.deallocate(h);
    }

    auto massive_handle = allocator.allocate(
        1'024 * 1'024 * 31, storage::StorageAlignment::_4KiB);
    BOOST_TEST(
        massive_handle.is_valid(),
        "Failed to allocate massive block. TLSF blocks did not properly "
        "coalesce physically!");
}

BOOST_AUTO_TEST_SUITE_END()
