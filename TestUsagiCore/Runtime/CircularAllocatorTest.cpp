#include <gtest/gtest.h>
#include <Usagi/Runtime/Memory/Allocators/CircularAllocator.hpp>

using namespace usagi;

class CircularAllocatorTest
    : public testing::Test
    , public CircularAllocator
{
    std::vector<char> mManagedMemory;

protected:
    void SetUp() override
    {
        mManagedMemory.resize(1024);
        mMemory = MemoryView(mManagedMemory.data(), mManagedMemory.size());
        init();
    }
};

TEST_F(CircularAllocatorTest, Alloc)
{
    // alloc 0 bytes, expect exception
    EXPECT_THROW(allocate(0), std::bad_alloc);

    // head=0, tail=0, free=1024

    [[maybe_unused]]
    // alloc 16-8 bytes, head -> 0, tail -> 16
    const auto alloc0 = allocate(16 - 8);
    ASSERT_EQ(mHead, 0);
    ASSERT_EQ(mTail, 16);

    // head=0, tail=16, free=1008
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    [[maybe_unused]]
    // alloc 32-8 bytes, head -> 0, tail -> 16 + 32
    const auto alloc1 = allocate(32 - 8);
    ASSERT_EQ(mHead, 0);
    ASSERT_EQ(mTail, 16 + 32);

    // head=0, tail=48, free=976
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    [[maybe_unused]]
    // alloc 64-8 bytes, head -> 0, tail -> 16 + 32 + 64
    const auto alloc2 = allocate(64 - 8);
    ASSERT_EQ(mHead, 0);
    ASSERT_EQ(mTail, 16 + 32 + 64);

    // head=0, tail=240, free=784
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    [[maybe_unused]]
    // alloc 128-8 bytes, head -> 0, tail -> 16 + 32 + 64 + 128
    const auto alloc3 = allocate(128 - 8);
    ASSERT_EQ(mHead, 0);
    ASSERT_EQ(mTail, 16 + 32 + 64 + 128);

    // head=0, tail=496, free=528
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    [[maybe_unused]]
    // alloc 256-8 bytes, head -> 0, tail -> 16 + 32 + 64 + 128 + 256 
    const auto alloc4 = allocate(256 - 8);
    ASSERT_EQ(mHead, 0);
    ASSERT_EQ(mTail, 16 + 32 + 64 + 128 + 256);

    // free space left: 1024 - 496 - header size.
    // ASSERT_TRUE(can_tail_allocate(1024 - 496 - 8)); // 520 bytes free
    // ASSERT_FALSE(can_tail_allocate(1024 - 496 - 7));
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    // deallocate the two in-between, head and tail shouldn't be affected.
    deallocate(alloc1);

    // head=0, used=16, free=32, used=448, tail=496, free=528
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    deallocate(alloc2);

    // head=0, used=16, free=96, used=384, tail=496, free=528
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    // merge_free_segments_from_head();
    ASSERT_EQ(mHead, 0);
    ASSERT_EQ(mTail, 16 + 32 + 64 + 128 + 256);

    // deallocate the one blocking the above two, there should be 16 + 32 + 64
    // free space on the head
    deallocate(alloc0);

    // head=0, free=112, used=384, tail=496, free=528
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    // merge_free_segments_from_head();

    // free=112, head=112, used=384, tail=496, free=528

    ASSERT_EQ(mHead, 16 + 32 + 64);
    ASSERT_EQ(mTail, 16 + 32 + 64 + 128 + 256);

    // exhaust the space on tail leaving only 9 byte
    [[maybe_unused]]
    const auto alloc5 = allocate(528 - 8 - 9);

    // free=112, head=112, used=903, tail=1015, free=9
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    ASSERT_EQ(mHead, 16 + 32 + 64);
    ASSERT_EQ(mTail, 1024 - 9);

    // // exhaust the space on tail leaving only one byte
    // [[maybe_unused]]
    // const auto alloc6 = allocate(1);
    //
    // // free=112, head=112, used=912, tail=1024, free=0
    // print(std::cerr);
    // ASSERT_TRUE(check_integrity());
    //
    // ASSERT_EQ(mHead, 16 + 32 + 64);
    // ASSERT_EQ(mTail, 0);

    // force a wrapping to beginning, the first three freed blocks should be
    // used
    [[maybe_unused]]
    const auto alloc7 = allocate(16 + 32); // 56

    // used=56, tail=56, free=56, head=112, free=56, used=912
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    ASSERT_EQ(mHead, 16 + 32 + 64);
    ASSERT_EQ(mTail, 16 + 32 + 8);

    // 56 bytes between the tail and head. 48 bytes available at max.
    // ASSERT_TRUE(can_tail_allocate(56 - 8));
    // ASSERT_FALSE(can_tail_allocate(56 - 7));

    deallocate(alloc3);
    print(std::cerr);
    ASSERT_TRUE(check_integrity());
    deallocate(alloc4);
    print(std::cerr);
    ASSERT_TRUE(check_integrity());
    deallocate(alloc5);
    print(std::cerr);
    ASSERT_TRUE(check_integrity());
    // deallocate(alloc6);
    // print(std::cerr);
    // ASSERT_TRUE(check_integrity());
    deallocate(alloc7);
    print(std::cerr);
    ASSERT_TRUE(check_integrity());

    ASSERT_EQ(mHead, 0);
    ASSERT_EQ(mTail, 0);
}
