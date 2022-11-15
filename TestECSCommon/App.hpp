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
    void generate_entities(auto &&value_range, auto &&initializer)
    {
        Archetype<ArchetypeComponents...> archetype;
        for(auto &&value : value_range)
        {
            initializer(
                value,
                archetype.template component<ArchetypeComponents>()...
            );
        }
    }

    void validate_entity_range(auto &&range, auto &&validator)
    {
        for(auto &&e : range)
        {
            validator(e);
        }
    }
}
;
}
