#include <fmt/ostream.h>

#include <Usagi/Modules/Algorithms/Statistics/RandomNumbers/ServiceRandomNumberGenerator.hpp>
#include <Usagi/Modules/Algorithms/Statistics/Sampling/Sampling/Roulette/SystemStochasticUniversalSamplingUnordered.hpp>
#include <Usagi/Modules/Runtime/KeyValueStorage/ServiceRuntimeKeyValueStorage.hpp>

#include "ProbabilityCommon.hpp"

using namespace usagi;

namespace
{
// ********************************************************************* //    
//                               Components                              //
// ********************************************************************* //

struct CDiscreteProbability
{
    double probability;
};

struct CEntitySample
{
    EntityId id;
};

// ********************************************************************* //    
//                                Systems                                //
// ********************************************************************* //

using SysStochasticUniversalSampling = SystemStochasticUniversalSamplingUnordered<
    CQuery<C<CDiscreteProbability>, C<CEntitySample>>,
    CDiscreteProbability,
    Archetype<CEntitySample>,
    CEntitySample
>;

// ********************************************************************* //    
//                                  App                                  //
// ********************************************************************* //

struct Services
    : ServiceRuntimeKeyValueStorage
    , ServiceThreadLocalRNG
{};

using SamplingTest = ProbabilityTestBase<
    C<CDiscreteProbability, CEntitySample>,
    Services,
    SysStochasticUniversalSampling
>;
}

TEST_F(SamplingTest, StochasticUniversalSamplingTest)
{
    constexpr std::size_t n_pop = 4;
    constexpr std::size_t n_samples = 20;

    service<ServiceRuntimeKeyValueStorage>().ensure<
        std::size_t
    >("target_sample_size") = n_samples;

    Archetype<CDiscreteProbability> archetype;

    std::array<EntityId, n_pop> ids;
    archetype(C<CDiscreteProbability>()).probability = 1;
    ids[0] = db.insert(archetype);
    archetype(C<CDiscreteProbability>()).probability = 4;
    ids[1] = db.insert(archetype);
    archetype(C<CDiscreteProbability>()).probability = 2;
    ids[2] = db.insert(archetype);
    archetype(C<CDiscreteProbability>()).probability = 3;
    ids[3] = db.insert(archetype);

    const std::array samples {
        ids[0], ids[0],
        ids[1], ids[1], ids[1], ids[1], ids[1], ids[1], ids[1], ids[1],
        ids[2], ids[2], ids[2], ids[2],
        ids[3], ids[3], ids[3], ids[3], ids[3], ids[3],
    };

    update_system<SysStochasticUniversalSampling>();

    std::size_t i = 0;
    for(auto &&e : filtered_range(CQuery<C<CEntitySample>>()))
    {
        fmt::print("sample: {}\n", e(C<CEntitySample>()).id);
        EXPECT_EQ(samples[i], e(C<CEntitySample>()).id);
        ++i;
    }
    EXPECT_EQ(i, n_samples);
}
