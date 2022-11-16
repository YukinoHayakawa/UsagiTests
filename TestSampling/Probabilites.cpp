#include <range/v3/view/generate_n.hpp>
#include <fmt/format.h>

#include <Usagi/Modules/Common/Indexing/EntityIndexDescriptor.hpp>
#include <Usagi/Modules/Common/Indexing/SystemRebuildEntityIndex.hpp>
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
    std::uniform_real_distribution dist(min_fitness, max_fitness);

    // initialize fitness values
    generate_entities<CFitnessValue>(
        ranges::views::generate_n([&] { return dist(rng); }, n_pop),
        [](auto &&val, auto &&fitness) { fitness.value = val; },
        n_pop
    );

    fmt::print("init:\n");
    expect_value_range(
        filtered_range_single_component<CFitnessValue>(CQuery<C<CFitnessValue>>()),
        [](const CFitnessValue &val) {
            fmt::print("{}\n", val.value); return val.value;
        },
        min_fitness, true,// [min, max)
        max_fitness, false,
        n_pop
    );

    // build fitness index
    update_system<SysRebuildFitnessIndex>();

    // visit entities in decreasing order of fitness
    fmt::print("sorted:\n");
    expect_monotonically_decreasing(
        indexed_key_range<IndexFitnessDescending>(),
        [](auto &&key) { fmt::print("{}\n", key); return key; },
        false,
        n_pop
    );

    // generate probability distribution
    update_system<SysWriteExponentialProbability>();
    
    fmt::print("probabilities:\n");
    const auto accumulation = expect_monotonically_increasing(
        indexed_component_range<IndexFitnessDescending, CSelectionProbability>(),
        [&](const CSelectionProbability &prob) {
            fmt::print("{}\n", prob.probability);
            return prob.probability;
        },
        false, // don't require strictly increasing
        n_pop
    );
    fmt::print("accumulated: {}\n", accumulation);
    // todo bug accumulation might not be exactly 1.0. how to handle SUS?
    EXPECT_DOUBLE_EQ(accumulation, 1.0);
}
