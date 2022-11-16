#include <Usagi/Modules/Common/Indexing/EntityIndexDescriptor.hpp>
#include <Usagi/Modules/Common/Indexing/SystemRebuildEntityIndex.hpp>

#include <range/v3/view/generate_n.hpp>
#include <fmt/format.h>

#include <Usagi/Modules/Algorithms/Statistics/Sampling/Probabilities/Ranking/SystemSelectionProbabilityRankingExponential.hpp>

#include "ProbabilityCommon.hpp"

using namespace usagi;

namespace
{

// ********************************************************************* //    
//                               Component                               //
// ********************************************************************* //

struct CFitnessValue
{
    double value;
};

struct CSelectionProbability
{
    double probability;
};

// ********************************************************************* //    
//                                Indices                                //
// ********************************************************************* //

using IndexFitnessDescending = EntityIndexDescriptor<
    CQuery<C<CFitnessValue>>,
    CFitnessValue,
    decltype([](auto &&c) { return c.value; }),
    std::greater // sort in descending order
>;

// ********************************************************************* //    
//                               Systems                                 //
// ********************************************************************* //

using SysRebuildFitnessIndex = SystemRebuildEntityIndex<IndexFitnessDescending>;
using SysWriteExponentialProbability =
    SystemSelectionProbabilityRankingExponential<
        IndexFitnessDescending,
        CSelectionProbability
    >;

// ********************************************************************* //    
//                                 App                                   //
// ********************************************************************* //

struct Services
    : ServiceExternalEntityIndex<IndexFitnessDescending>
{};

using ProbabilityTest = ProbabilityTestBase<
    C<CFitnessValue, CSelectionProbability>,
    Services,
    SysRebuildFitnessIndex,
    SysWriteExponentialProbability
>;
}

// ********************************************************************* //    
//                                Tests                                  //
// ********************************************************************* //

TEST_F(ProbabilityTest, ExponentialScalingUnityTest)
{
    constexpr double min_fitness = 0, max_fitness = 100;
    constexpr std::size_t n_pop = 64;
    
    std::mt19937_64 rng { std::random_device()() };
    std::uniform_real_distribution<> dist(min_fitness, max_fitness);

    generate_entities<CFitnessValue>(
        ranges::views::generate_n([&] { return dist(rng); }, n_pop),
        [](auto &&val, auto &&fitness) { fitness.value = val; },
        n_pop
    );

    fmt::print("init:\n");
    // validate_entity_range(
    //     filtered_range(CQuery<C<CFitnessValue>>()),
    //     [&] (auto &&e) {
    //         const auto val = e(C<CFitnessValue>()).value;
    //         fmt::print("{}\n", val);
    //         EXPECT_GE(val, min_fitness);
    //         EXPECT_LT(val, max_fitness);
    //     }, n_pop
    // );

    expect_value_range(
        filtered_range_single_component<CFitnessValue>(CQuery<C<CFitnessValue>>()),
        [](const CFitnessValue &val) {
            fmt::print("{}\n", val.value); return val.value;
        },
        min_fitness, true,
        max_fitness, false,
        n_pop
    );

    update_system<SysRebuildFitnessIndex>();

    fmt::print("sorted:\n");
    // validate_entity_range(
    //     get_index(IndexFitnessDescending()).index,
    //     [max = std::numeric_limits<double>::max()](std::pair<double, EntityId> pair) 
    //         mutable {
    //         fmt::print("{}\n", pair.first);
    //         EXPECT_GE(max, pair.first);
    //         max = pair.first;
    //     }, n_pop
    // );
    expect_monotonically_decreasing(
        indexed_key_range<IndexFitnessDescending>(),
        [](auto &&key) { fmt::print("{}\n", key); return key; },
        false,
        n_pop
    );

    update_system<SysWriteExponentialProbability>();
    
    fmt::print("probabilities:\n");

    const auto accumulation = expect_monotonically_increasing(
        indexed_component_range<IndexFitnessDescending, CSelectionProbability>(),
        [&](const CSelectionProbability &prob) {
            fmt::print("{}\n", prob.probability);
            return prob.probability;
        },
        true,
        n_pop
    );
    fmt::print("accumulated: {}\n", accumulation);
    // todo bug accumulation might not be exactly 1.0. how to handle SUS?
    EXPECT_DOUBLE_EQ(accumulation, 1.0);
    //
    // double accumulated = 0;
    // validate_entity_range(
    //     [&, min = std::numeric_limits<double>::min()](std::pair<double, EntityId> pair) mutable {
    //         auto view = db.entity_view<ComponentAccessAllowAll>(pair.second);
    //         ASSERT_TRUE(view.include(C<CSelectionProbability>()));
    //         auto prob = view(C<CSelectionProbability>()).probability;
    //         fmt::print("{} -> {}\n", pair.first, prob);
    //         EXPECT_LE(min, prob);
    //         min = prob;
    //         accumulated += prob;
    //     }, n_pop
    // );
    // fmt::print("accumulated: {}\n", accumulated);
    // // todo bug accumulation might not be exactly 1.0. how to handle SUS?
    // EXPECT_DOUBLE_EQ(accumulated, 1.0);
}
