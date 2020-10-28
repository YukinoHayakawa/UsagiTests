#define _CRT_SECURE_NO_WARNINGS

#include <gtest/gtest.h>

#include <Usagi/Runtime/Memory/VmAllocatorFileBacked.hpp>
#include <Usagi/Runtime/Memory/VmAllocatorPagefileBacked.hpp>

using namespace usagi;

#define PAGE_SIZE 4096

TEST(VmAllocator, FileBackedAllocation)
{
    const std::filesystem::path file { (char8_t*)tmpnam(nullptr) };

    VmAllocatorFileBacked alloc;

    // allocation cannot be done without a backing file
    EXPECT_EQ(alloc.max_size(), 0);
    ASSERT_THROW(alloc.allocate(PAGE_SIZE), std::bad_alloc);

    alloc.set_backing_file(file);

    // reallocated fallback to allocate if nullptr is passed in
    void *ptr;
    ASSERT_NO_THROW(ptr = alloc.reallocate(nullptr, PAGE_SIZE));
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(std::filesystem::file_size(file), PAGE_SIZE);

    // no double allocation
    ASSERT_THROW(alloc.allocate(PAGE_SIZE), std::bad_alloc);

    // reallocation
    auto ptr2 = alloc.reallocate(ptr, PAGE_SIZE * 2);
    EXPECT_EQ(std::filesystem::file_size(file), PAGE_SIZE * 2);

    // deallocation
    alloc.deallocate(ptr2);

    // no double deallocation
    ASSERT_THROW(alloc.deallocate(ptr2), std::bad_alloc);

    // reallocation after deallocation
    ASSERT_NO_THROW(ptr2 = alloc.allocate(PAGE_SIZE * 3));
    ASSERT_NO_THROW(alloc.deallocate(ptr2));

}

TEST(VmAllocator, PagefileBackedAllocation)
{
    VmAllocatorPagefileBacked alloc;

    alloc.reserve(PAGE_SIZE * 8);
    EXPECT_EQ(alloc.max_size(), PAGE_SIZE * 8);

    auto ptr = alloc.allocate(PAGE_SIZE * 1);
    // no double allocation
    ASSERT_THROW(alloc.allocate(PAGE_SIZE), std::bad_alloc);

    auto ptr2 = (unsigned char *)alloc.reallocate(ptr, PAGE_SIZE * 4);
    ASSERT_EQ(ptr, ptr2);

    // fill with 1s
    memset(ptr2, 1, PAGE_SIZE * 4);
    alloc.zero_memory(ptr2, 2048, PAGE_SIZE * 3);

    ASSERT_EQ(ptr2[2047], 1);
    ASSERT_EQ(ptr2[2048], 0);
    ASSERT_EQ(ptr2[PAGE_SIZE], 0); // begin of 2nd page
    ASSERT_EQ(ptr2[PAGE_SIZE * 2], 0); // begin of 3rd page
    ASSERT_EQ(ptr2[PAGE_SIZE * 3], 0); // begin of 4th page
    ASSERT_EQ(ptr2[PAGE_SIZE * 3 + 2047], 0);
    ASSERT_EQ(ptr2[PAGE_SIZE * 3 + 2048], 1);

    ASSERT_NO_THROW(alloc.deallocate(ptr2));
}
