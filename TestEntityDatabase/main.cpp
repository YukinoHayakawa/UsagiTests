#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <Usagi/Game/Database/EntityDatabase.hpp>
#include <Usagi/Game/Entity/Archetype.hpp>
#include <Usagi/Game/detail/ComponentAccessSystemAttribute.hpp>
#include <Usagi/Game/detail/EntityDatabaseAccessExternal.hpp>

using namespace usagi;

struct ComponentA
{
};

using Archetype1 = Archetype<ComponentA>;
using Database1 = EntityDatabase<Archetype1::ComponentFilterT>;

TEST(EntityDatabaseTest, ArchetypePageReuse)
{
    Database1 db;
    Archetype1 a;

    const auto i = db.create(a);
    // allocated new page
    EXPECT_EQ(i.id, 0);

    db.entity_view(i).destroy();
    const auto i2 = db.create(a);
    // page reused
    EXPECT_EQ(i2.id, 1);
    EXPECT_EQ(i2.page_idx, i.page_idx);

    db.entity_view(i2).destroy();
    // page got deallocated
    db.reclaim_pages();

    const auto i3 = db.create(a);
    // page reused on the same memory
    EXPECT_EQ(i3.id, Database1::ENTITY_PAGE_SIZE);
    EXPECT_EQ(i2.page_idx, i.page_idx);
}
