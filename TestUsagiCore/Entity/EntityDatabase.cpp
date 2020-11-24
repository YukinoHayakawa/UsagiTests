#include <gtest/gtest.h>

#include <Usagi/Entity/EntityDatabase.hpp>
#include <Usagi/Entity/detail/ComponentFilter.hpp>

using namespace usagi;

namespace
{
USAGI_DECL_TAG_COMPONENT(ComponentTag);

using EnabledComponents = ComponentFilter<
    ComponentTag
>;
using Database = EnabledComponents::apply<EntityDatabaseInMemory>;
}

#define UPDATE_SYSTEM(sys) \
    sys.update(rt, EntityDatabaseAccessExternal< \
        App::DatabaseT, \
        ComponentAccessSystemAttribute<decltype(sys)> \
    >(&db)) \
/**/

TEST(EntityDatabaseTest, FilteredView)
{
    Database db;

    Archetype archetype;

    // Add tags
    for(int i = 0; i < 4 * 32 + 16; ++i)
    {
        const auto id = db.insert(archetype);
        if(id.offset % 2 == 0 && id.page % 2 == 1)
            db.entity_view<ComponentAccessAllowAll>(id)
                .add_component(ComponentTag());
    }
    // Count tags
    auto access = db.create_access<ComponentAccessAllowAll>();
    {
        auto range = access.view();
        const auto c = std::count_if(range.begin(), range.end(), [](auto &&e) {
            return e.has_component(ComponentTag());
        });
        EXPECT_EQ(c, 32);
    }
    {
        auto range = access.unfiltered_view();
        const auto c = std::count_if(range.begin(), range.end(), [](auto &&e) {
            return e.has_component(ComponentTag());
        });
        EXPECT_EQ(c, 32);
    }
    {
        auto view = access.view(EnabledComponents());
        const auto c = std::distance(view.begin(), view.end());
        EXPECT_EQ(c, 32);
    }
    // Clear tags
    {
        int counter = 0;
        for(auto &&e : access.view(EnabledComponents()))
        {
            e.remove_component(ComponentTag());
            ++counter;
        }
        EXPECT_EQ(counter, 32);
    }
    {
        auto range = access.unfiltered_view();
        const auto c = std::count_if(range.begin(), range.end(), [](auto &&e) {
            return e.has_component(ComponentTag());
            });
        EXPECT_EQ(c, 0);
    }
    {
        auto view = access.view(EnabledComponents());
        const auto c = std::distance(view.begin(), view.end());
        EXPECT_EQ(c, 0);
    }
    // Add tags
    for(int i = 0; i < 4 * 32 + 16; ++i)
    {
        for(auto &&e : access.view())
        {
            const auto id = e.id();
            if(id.offset % 2 == 1 && id.page % 2 == 0)
                e.add_component(ComponentTag());
        }
    }
    // Count tags again
    {
        auto range = access.view();
        const auto c = std::count_if(range.begin(), range.end(), [](auto &&e) {
            return e.has_component(ComponentTag());
        });
        EXPECT_EQ(c, 40);
    }
    {
        auto range = access.unfiltered_view();
        const auto c = std::count_if(range.begin(), range.end(), [](auto &&e) {
            return e.has_component(ComponentTag());
        });
        EXPECT_EQ(c, 40);
    }
    {
        auto view = access.view(EnabledComponents());
        const auto c = std::distance(view.begin(), view.end());
        EXPECT_EQ(c, 40);
    }
}
