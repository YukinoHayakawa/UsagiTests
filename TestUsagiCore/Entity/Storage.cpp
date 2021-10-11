﻿#include <gtest/gtest.h>

#include <Usagi/Entity/EntityDatabase.hpp>

using namespace usagi;

namespace
{
USAGI_DECL_TAG_COMPONENT(ComponentA);

struct ComponentB
{
};

using WriteFilter = ComponentFilter<ComponentA, ComponentB>;
using Archetype1 = Archetype<>;

/*
struct SystemA
{
    using WriteAccess = WriteFilter;
    using ReadAccess = ComponentFilter<>;

    template <typename RuntimeServices, typename EntityDatabaseAccess>
    void update(RuntimeServices &&rt, EntityDatabaseAccess &&db)
    {
    }
};
*/

using Database1 = WriteFilter::apply<EntityDatabaseDefaultConfig>;
}

TEST(EntityDatabaseStotrgeTest, FlagComponent)
{
    Database1 db;

    static_assert(std::is_base_of_v<Tag<ComponentA>, Database1>);
    static_assert(!std::is_base_of_v<Tag<ComponentB>, Database1>);

    // auto access = db.create_access<ComponentAccessSystemAttribute<SystemA>>();
    // auto view = access.view(WriteFilter());
    Archetype1 archetype;
    auto id = db.insert(archetype);
    auto e = db.entity_view<ComponentAccessAllowAll>(id);

    EXPECT_FALSE(e.include<ComponentA>());
    EXPECT_FALSE(e.include<ComponentB>());

    e.add_component<ComponentA>();
    EXPECT_TRUE(e.include<ComponentA>());

    e.add_component<ComponentB>();
    EXPECT_TRUE(e.include<ComponentB>());

    e.remove_component<ComponentA>();
    EXPECT_FALSE(e.include<ComponentA>());

    e.remove_component<ComponentB>();
    EXPECT_FALSE(e.include<ComponentB>());
}
