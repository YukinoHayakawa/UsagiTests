#pragma once

#include <gtest/gtest.h>

#include <tuple>

#include <Usagi/Entity/Component.hpp>
#include <Usagi/Entity/EntityDatabase.hpp>
#include <Usagi/Entity/System.hpp>

namespace usagi
{
template <
    SimpleComponentFilter EnabledComponents,
    typename Services,
    System... Systems
>
struct TestApp : ::testing::Test
{
    using Database =
        typename EnabledComponents::template apply<EntityDatabaseDefaultConfig>;

    Database db;
    Services runtime;
    std::tuple<Systems...> systems;

    template <std::size_t Idx>
    void update_system()
    {
        std::get<Idx>(systems).update(
            runtime,
            db.create_access<ComponentAccessSystemAttribute<
                std::tuple_element_t<Idx, decltype(systems)>
            >()
        );
    }

    template <typename Sys>
    void update_system()
    {
        std::get<Sys>(systems).update(
            runtime,
            db.template create_access<ComponentAccessSystemAttribute<Sys>>()
        );
    }

    auto filtered_range(SimpleComponentQuery auto query)
    {
        return db.template create_access<ComponentAccessAllowAll>()
            .view(query);
    }

    template <Component... ArchetypeComponents>
    void generate_entities(
        auto &&value_range,
        auto &&initializer,
        const std::size_t expect_generated_entities = -1)
    {
        std::size_t num_inserted = 0;
        Archetype<ArchetypeComponents...> archetype;
        for(auto &&value : value_range)
        {
            initializer(
                value,
                archetype.template component<ArchetypeComponents>()...
            );
            db.insert(archetype);
            ++num_inserted;
        }
        if(expect_generated_entities != -1)
            EXPECT_EQ(expect_generated_entities, num_inserted);
    }

    void validate_entity_range(
        auto &&range,
        auto &&validator,
        const std::size_t expect_visited_entities = -1)
    {
        std::size_t count = 0;
        for(auto &&e : range)
        {
            validator(e);
            ++count;
        }
        if(expect_visited_entities != -1) 
            EXPECT_EQ(expect_visited_entities, count);
    }

    void validate_entity_range_size(auto &&range, std::size_t size)
    {
        auto begin = range.begin();
        auto end = range.end();
        EXPECT_EQ(std::distance(begin, end), size);
        // todo EXPECT_EQ(std::ranges::size(range), size);
    }
}
;
}
