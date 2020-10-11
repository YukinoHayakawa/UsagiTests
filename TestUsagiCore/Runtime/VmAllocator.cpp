#define _CRT_SECURE_NO_WARNINGS

#include <gtest/gtest.h>

#include <Usagi/Concept/Type/Memcpyable.hpp>
#include <Usagi/Runtime/Memory/VmAllocatorFileBacked.hpp>

using namespace usagi;

#define PAGE_SIZE 4096

TEST(VmAllocator, FileBackedAllocation)
{
    const std::filesystem::path file { (char8_t*)tmpnam(nullptr) };

    VmAllocatorFileBacked alloc;

    // file path not set
    EXPECT_EQ(alloc.max_size(), 0);
    ASSERT_THROW(alloc.allocate(PAGE_SIZE), std::bad_alloc);

    alloc.set_backing_file(file);

    ASSERT_THROW(alloc.reallocate(nullptr, PAGE_SIZE), std::bad_alloc);

    // first allocation
    auto ptr = alloc.allocate(PAGE_SIZE);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(std::filesystem::file_size(file), PAGE_SIZE);

    // no double allocation
    ASSERT_THROW(alloc.allocate(PAGE_SIZE), std::bad_alloc);

    // reallocation
    auto ptr2 = alloc.reallocate(ptr, PAGE_SIZE * 2);
    EXPECT_EQ(std::filesystem::file_size(file), PAGE_SIZE * 2);

    // deallocation
    alloc.deallocate(ptr2);

    ASSERT_THROW(alloc.reallocate(nullptr, PAGE_SIZE), std::bad_alloc);
    ASSERT_THROW(alloc.deallocate(ptr2), std::bad_alloc);

    auto ptr3 = alloc.allocate(PAGE_SIZE * 3);
    alloc.deallocate(ptr3);
}
