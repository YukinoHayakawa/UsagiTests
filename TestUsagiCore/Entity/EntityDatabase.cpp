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
            return e.include(ComponentTag());
        });
        EXPECT_EQ(c, 32);
    }
    {
        auto range = access.unfiltered_view();
        const auto c = std::count_if(range.begin(), range.end(), [](auto &&e) {
            return e.include(ComponentTag());
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
            return e.include(ComponentTag());
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
            return e.include(ComponentTag());
        });
        EXPECT_EQ(c, 40);
    }
    {
        auto range = access.unfiltered_view();
        const auto c = std::count_if(range.begin(), range.end(), [](auto &&e) {
            return e.include(ComponentTag());
        });
        EXPECT_EQ(c, 40);
    }
    {
        auto view = access.view(EnabledComponents());
        const auto c = std::distance(view.begin(), view.end());
        EXPECT_EQ(c, 40);
    }
}

TEST(EntityDatabaseTest, Sampling)
{
    Database db;
    std::mt19937 rng(std::random_device{}());
    auto access = db.create_access<ComponentAccessAllowAll>();

    // No sample obtained when the database is empty
    for(int i = 0; i < 100; ++i)
    {
        auto sample = access.sample_random_access_single(rng);
        EXPECT_FALSE(sample.has_value());
    }

    Archetype<ComponentTag> archetype;
    for(int i = 0; i < 10; ++i)
    {
        db.insert(archetype);
    }

    archetype = {};
    for(int i = 0; i < 10; ++i)
    {
        db.insert(archetype);
    }

    auto all_trials = access.create_sampling_counter();
    const int rounds = 128;
    for(int i = 0; i < rounds; ++i)
    {
        auto trails = access.create_sampling_counter();
        auto sample = access.sample_random_access_single(rng, {}, {}, -1);
        ASSERT_TRUE(sample.has_value());
        EXPECT_TRUE(sample->include<ComponentTag>());
        sample = access.sample_random_access_single(
            rng, C<ComponentTag>(), {}, -1, &trails
        );
        ASSERT_TRUE(sample.has_value());
        all_trials += trails;
        EXPECT_TRUE(sample->include<ComponentTag>());
        sample = access.sample_random_access_single(
            rng, {}, C<ComponentTag>()
        );
        EXPECT_FALSE(sample.has_value());
    }

    // note that the succeed trait is not included in these counts!
    std::cout << "avg num failed trials:"
        << "\n    !valid      : " << all_trials.num_invalid / rounds
        << "\n    !include    : " << all_trials.num_include_unsatisfied / rounds
        << "\n    !exclude    : " << all_trials.num_exclude_unsatisfied / rounds
        << "\n" << std::endl;

    // https://www.geeksforgeeks.org/expected-number-of-trials-before-success/
    // https://www.cut-the-knot.org/Probability/LengthToFirstSuccess.shtml
    auto range = access.view(C<ComponentTag>());
    auto range2 = access.unfiltered_view();
    const auto population_size = std::distance(range.begin(), range.end());
    const auto pool_size = std::distance(range2.begin(), range2.end());
    const auto hit_expectation = (double)population_size / pool_size;
    const auto attempt_expectation = 1 / hit_expectation;

    std::cout << "expectations:"
        << "\n    population  : " << population_size
        << "\n    pool        : " << pool_size
        << "\n    hit         : " << hit_expectation
        << "\n    num attempts: " << attempt_expectation
        << "\n" << std::endl;
}
