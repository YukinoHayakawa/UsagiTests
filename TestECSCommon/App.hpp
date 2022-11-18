#pragma once

#include <gtest/gtest.h>

#include <tuple>
#include <ranges>

#include <Usagi/Entity/Component.hpp>
#include <Usagi/Entity/EntityDatabase.hpp>
#include <Usagi/Entity/System.hpp>
#include <Usagi/Modules/Common/Indexing/ServiceExternalEntityIndex.hpp>

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

    // ********************************************************************* //    
    //                                Executive                              //
    // ********************************************************************* //

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

    template <typename ServiceT>
    auto & service()
    {
        return static_cast<ServiceT &>(runtime).get_service();
    }

    template <typename IndexDescriptor>
    auto & get_index(IndexDescriptor index)
    {
        return ServiceAccess<
            ServiceExternalEntityIndex<IndexDescriptor>
        >(runtime).template entity_index<IndexDescriptor>();
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

    void expect_entity_range_size(auto &&range, std::size_t size)
    {
        auto begin = range.begin();
        auto end = range.end();
        EXPECT_EQ(std::distance(begin, end), size);
        // todo EXPECT_EQ(std::ranges::size(range), size);
    }

    // ********************************************************************* //    
    //                                 Ranges                                //
    // ********************************************************************* //

    auto filtered_range(SimpleComponentQuery auto query)
    {
        return db.template create_access<ComponentAccessAllowAll>()
            .view(query);
    }

    template <Component Comp>
    auto filtered_range_single_component(SimpleComponentQuery auto query)
    {
        return filtered_range(query) |
            std::views::transform([](auto &&e) -> const Comp & {
                return e(C<Comp>());
            });
    }

    template <typename IndexDescriptor, Component Comp>
    auto indexed_component_range()
    {
        const auto proj = [this](const auto &pair) -> const Comp &
        {
            auto access = 
                db.template create_access<ComponentAccessAllowAll>();
            auto view = access.entity(pair.second);
            const bool has_comp = view.template include<Comp>();
            // ASSERT_TRUE(has_comp);
            assert(has_comp);
            return view(C<Comp>());
        };

        return get_index(IndexDescriptor()).index |
            std::views::transform(proj);
    }

    template <typename IndexDescriptor>
    auto indexed_key_range()
    {
        const auto proj = [](const auto &pair)
        {
            return pair.first;
        };

        return get_index(IndexDescriptor()).index |
            std::views::transform(proj);
    }

    // ********************************************************************* //    
    //                               Generation                              //
    // ********************************************************************* //
    
    template <Component... ArchetypeComponents>
    std::size_t generate_entities(
        auto &&generator,
        auto &&initializer,
        const std::optional<std::size_t> expected_generated_entities = { })
    {
        std::size_t num_inserted = 0;
        Archetype<ArchetypeComponents...> archetype;
        for(auto &&value : generator)
        {
            initializer(
                value,
                archetype.template component<ArchetypeComponents>()...
            );
            db.insert(archetype);
            ++num_inserted;
        }
        if(expected_generated_entities)
            EXPECT_EQ(*expected_generated_entities, num_inserted);
        return num_inserted;
    }

    // ********************************************************************* //    
    //                                Numerical                              //
    // ********************************************************************* //

    template <std::floating_point Val>
    struct NumericalStat
    {
        std::size_t num = 0;
        Val sum = 0;
        Val mean = 0;
        Val variance = 0;
        Val std_dev = 0;
    };

    template <std::floating_point Val>
    auto expect_value_range(
        auto &&range,
        auto &&projection,
        const Val min,
        const bool closed_left,
        const Val max,
        const bool closed_right,
        const std::optional<std::size_t> expected_num_entities = { })
    {
        NumericalStat<Val> stat;

        for(auto &&val : range)
        {
            const auto projected = projection(val);
            if(closed_left) EXPECT_GE(projected, min);
            else EXPECT_GT(projected, min);
            if(closed_right) EXPECT_LE(projected, max);
            else EXPECT_LT(projected, max);
            stat.sum += projected;
            ++stat.num;
        }

        stat.mean = stat.sum / static_cast<Val>(stat.num);

        for(auto &&val : range)
        {
            const auto projected = projection(val);
            stat.variance += std::pow(projected - stat.mean, 2);
        }
        stat.variance /= static_cast<Val>(stat.num);
        stat.std_dev = std::sqrt(stat.variance);

        if(expected_num_entities)
            EXPECT_EQ(*expected_num_entities, stat.num);

        return stat;
    }
};
}
