#include <gtest/gtest.h>
#include <fmt/ostream.h>

#include <Usagi/Entity/EntityDatabase.hpp>
#include <Usagi/Entity/detail/ComponentFilter.hpp>
#include <Usagi/Modules/Common/Functional/Visitors/SystemVisitEntities.hpp>
#include <Usagi/Modules/Common/Functional/Visitors/Transformative.hpp>

#include "../TestECSCommon/App.hpp"

#include "Components.hpp"

using namespace usagi;

struct OpDoubleValue
{
    auto operator()(const auto &lhs, auto &rhs)
    {
        rhs.value = lhs.value * 2;
    }
};

using SysDoubleValues = SystemVisitEntities<
    CQuery<C<CPreTransformValue<int>>>, // query filter
    EntityVisitorTransformSingleComponent< // op
        CPreTransformValue<int>,
        CPostTransformValue<int>,
        OpDoubleValue
    >
>;

struct RuntimeServices
{
    
};

using TransformTest = TestApp<
    C<CPreTransformValue<int>, CPostTransformValue<int>>,
    RuntimeServices,
    SysDoubleValues
>;

TEST_F(TransformTest, DoubleValueTest)
{
    constexpr std::array init_values = { 1, 2, 3, 4, 5 };

    generate_entities<CPreTransformValue<int>>(init_values, 
        [](auto &&init_val, auto &c0) {
            c0.value = init_val;
        }, 5);

    validate_entity_range(
        filtered_range(CQuery<C<CPreTransformValue<int>>>()),
        [&, i = 0] (auto &&e) mutable {
            EXPECT_EQ(e(C<CPreTransformValue<int>>()).value, init_values[i++]);
        }, 5
    );

    update_system<SysDoubleValues>();

    validate_entity_range(
        filtered_range(CQuery<C<CPreTransformValue<int>>>()),
        [&, i = 0] (auto &&e) mutable {
            EXPECT_EQ(e(C<CPreTransformValue<int>>()).value, init_values[i]);
            EXPECT_EQ(e(C<CPostTransformValue<int>>()).value, 2 * init_values[i++]);
        }, 5
    );
}
