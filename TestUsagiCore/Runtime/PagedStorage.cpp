#define _CRT_SECURE_NO_WARNINGS

#include <gtest/gtest.h>

#include <Usagi/Entity/Component.hpp>
#include <Usagi/Runtime/Memory/PagedStorage.hpp>

using namespace usagi;

namespace
{
struct ComponentA
{
    std::uint64_t a;
    double b;
};
static_assert(Component<ComponentA>);
}

TEST(PagedStorageTest, FileBackedRestoration)
{
    const std::filesystem::path file = (char8_t*)tmpnam(nullptr);
    std::cout << file << std::endl;

    std::uint64_t size, cap, ia, ib;
    {
        PagedStorageFileBacked<ComponentA> pool { file };
        EXPECT_EQ(pool.size(), 0);
        EXPECT_EQ(pool.capacity(), 0);
    }
    // release empty storage

    // load it back
    {
        PagedStorageFileBacked<ComponentA> pool { file };
        EXPECT_EQ(pool.size(), 0);
        EXPECT_EQ(pool.capacity(), 0);

        ComponentA &a = pool.at(ia = pool.allocate());
        a.a = 0x12345678;
        a.b = 3.11456678;
        EXPECT_EQ(pool.size(), 1);
        EXPECT_GE(pool.capacity(), pool.size());

        pool.deallocate(pool.allocate());
        EXPECT_EQ(pool.size(), 2);
        EXPECT_GE(pool.capacity(), pool.size());

        ComponentA &b = pool.at(ib = pool.allocate());
        b.a = 0x87654321;
        b.b = 0.11155555;
        EXPECT_EQ(pool.size(), 2);
        EXPECT_GE(pool.capacity(), pool.size());

        size = pool.size();
        cap = pool.capacity();
    }
    // storage released

    // restore the state
    {
        PagedStorageFileBacked<ComponentA> pool { file };
        ASSERT_EQ(pool.size(), size);
        ASSERT_EQ(pool.capacity(), cap);

        ComponentA &a = pool.at(ia);
        EXPECT_EQ(a.a, 0x12345678);
        EXPECT_EQ(a.b, 3.11456678);

        ComponentA &b = pool.at(ib);
        EXPECT_EQ(b.a, 0x87654321);
        EXPECT_EQ(b.b, 0.11155555);

        EXPECT_NO_THROW(pool.allocate());
        EXPECT_EQ(pool.size(), 3);
    }
    // todo test unused storage
}
