#include <gtest/gtest.h>

#include <Usagi/Entity/EntityDatabase.hpp>
#include <Usagi/Entity/Archetype.hpp>
#include <Usagi/Entity/detail/ComponentAccessSystemAttribute.hpp>
#include <Usagi/Entity/detail/EntityDatabaseAccessExternal.hpp>

using namespace usagi;

namespace
{
struct ComponentA
{
    int i;
};

using Archetype1 = Archetype<ComponentA>;
using Database1 = Archetype1::ComponentFilterT::apply<
    EntityDatabaseDefaultConfig
>;

struct SystemA
{
    using WriteAccess = ComponentFilter<ComponentA>;
    using ReadAccess = ComponentFilter<>;

    template <typename RuntimeServices, typename EntityDatabaseAccess>
    void update(RuntimeServices &&rt, EntityDatabaseAccess &&db)
    {
    }
};
}

TEST(EntityDatabaseTest, ArchetypePageReuse)
{
    Database1 db;
    Archetype1 a;

    const EntityId i = db.insert(a);
    // allocated new page
    EXPECT_EQ(i.page, 0);

    db.entity_view<ComponentAccessAllowAll>(i).destroy();
    const EntityId i2 = db.insert(a);
    // page reused
    EXPECT_EQ(i2.offset, 1);
    EXPECT_EQ(i2.page, i.page);

    db.entity_view<ComponentAccessAllowAll>(i2).destroy();
    // page got deallocated
    db.reclaim_pages();

    const EntityId i3 = db.insert(a);
    // page reused on the same memory
    EXPECT_EQ(i3.offset, 0);
    EXPECT_EQ(i3.page, i.page);
}

class EntityDatabasePageTest : public ::testing::Test
{
public:
    Database1 db;
    EntityId id1 { };
    EntityId id2 { };
    EntityId id3 { };
    EntityDatabaseAccessExternal<
        Database1,
        ComponentAccessSystemAttribute<SystemA>
    > access { &db };

    void SetUp() override
    {
        {
            Archetype1 a;
            a.component<ComponentA>().i = 1;
            id1 = db.insert(a);
        }
        {
            Archetype1 a;
            a.component<ComponentA>().i = 2;
            id2 = db.insert(a);
        }
        {
            Archetype1 a;
            a.component<ComponentA>().i = 3;
            id3 = db.insert(a);
        }
        // each entity on a different page
        ASSERT_NE(id1.page, id2.page);
        ASSERT_NE(id3.page, id2.page);
    }
};

TEST_F(EntityDatabasePageTest, IterationOrder)
{
    auto view = access.view(Archetype1::ComponentFilterT());
    auto iter = view.begin();

    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 1);
    ++iter;
    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 2);
    ++iter;
    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 3);
    ++iter;
    EXPECT_EQ(iter, view.end());
}

TEST_F(EntityDatabasePageTest, DeleteFirstPage)
{
    db.entity_view<ComponentAccessAllowAll>(id1).destroy();
    db.reclaim_pages();

    auto view = access.view(Archetype1::ComponentFilterT());
    auto iter = view.begin();

    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 2);
    ++iter;
    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 3);
    ++iter;
    EXPECT_EQ(iter, view.end());
}

TEST_F(EntityDatabasePageTest, DeleteMiddlePage)
{
    db.entity_view<ComponentAccessAllowAll>(id2).destroy();
    db.reclaim_pages();

    auto view = access.view(Archetype1::ComponentFilterT());
    auto iter = view.begin();

    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 1);
    ++iter;
    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 3);
    ++iter;
    EXPECT_EQ(iter, view.end());
}

TEST_F(EntityDatabasePageTest, DeleteLastPage)
{
    db.entity_view<ComponentAccessAllowAll>(id3).destroy();
    db.reclaim_pages();

    auto view = access.view(Archetype1::ComponentFilterT());
    auto iter = view.begin();

    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 1);
    ++iter;
    ASSERT_TRUE((*iter).include<ComponentA>());
    EXPECT_EQ((*iter).component<ComponentA>().i, 2);
    ++iter;
    EXPECT_EQ(iter, view.end());
}

TEST_F(EntityDatabasePageTest, VoidFilteredIterator)
{
    db.entity_view<ComponentAccessAllowAll>(id2).destroy();

    auto unfiltered_view = access.unfiltered_view();

    // unfiltered iterators visit all entities regardless whether
    // they are really created
    EXPECT_EQ(
        std::distance(unfiltered_view.begin(), unfiltered_view.end()),
        3 * Database1::EntityPageT::PAGE_SIZE
    );

    // filtered range with an empty include filter visits created entities
    // including those not having components
    auto void_view = access.view();
    EXPECT_EQ(
        std::distance(void_view.begin(), void_view.end()),
        3
    );
}
