#define _CRT_SECURE_NO_WARNINGS

#include <filesystem>

#include <gtest/gtest.h>
#include <Usagi/Library/Container/DynamicArray.hpp>
#include <Usagi/Runtime/Memory/VmAllocatorFileBacked.hpp>

using namespace usagi;

TEST(DynamicArray, RestoreFromMmappedFile)
{
    const std::filesystem::path file { (char8_t*)tmpnam(nullptr) };
    const std::size_t num = 65535;
    std::size_t capacity;
    // init
    {
        VmAllocatorFileBacked alloc;
        alloc.set_backing_file(file);

        DynamicArray<uint64_t, VmAllocatorFileBacked> array(std::move(alloc));

        ASSERT_EQ(array.size(), 0);
        EXPECT_GE(array.capacity(), 0);

        // make sure the total allocation would be larger than 64 KiB
        // to force a reallocation
        for(std::size_t i = 0; i < num; ++i)
        {
            array.push_back(i);
        }

        ASSERT_EQ(array.size(), num);
        EXPECT_GE(array.capacity(), num);
        capacity = array.capacity();
    }
    // reload
    {
        VmAllocatorFileBacked alloc;
        alloc.set_backing_file(file);

        DynamicArray<uint64_t, VmAllocatorFileBacked> array(std::move(alloc));

        ASSERT_EQ(array.size(), num);
        ASSERT_EQ(capacity, array.capacity());

        for(std::size_t i = 0; i < num; ++i)
        {
            EXPECT_EQ(i, array[i]);
        }
    }
}
