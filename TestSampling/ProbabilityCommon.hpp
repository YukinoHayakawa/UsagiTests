#pragma once

#include "../TestECSCommon/App.hpp"

namespace usagi
{
template <
    SimpleComponentFilter EnabledComponents,
    typename Services,
    System... Systems
>
struct ProbabilityTestBase : TestApp<EnabledComponents, Services, Systems...>
{
    auto expect_monotonically_increasing(
        auto &&range,
        auto &&projection,
        const bool strict,
        const std::optional<std::size_t> expected_range_size)
    {
        using ValueT = std::remove_cvref_t<
            decltype(projection(*range.begin()))
        >;
        std::optional<ValueT> min;
        ValueT accumulation { };
        std::size_t n_visited = 0;
        for(auto &&val : range)
        {
            const ValueT projected = projection(val);
            accumulation += projected;
            if(min.has_value())
            {
                // strictly increasing
                if(strict) EXPECT_LT(min.value(), projected);
                // allow remaining constant
                else EXPECT_LE(min.value(), projected);
            }
            min = projected;
            ++n_visited;
        }
        if(expected_range_size)
        {
            EXPECT_EQ(*expected_range_size, n_visited);
        }
        return accumulation;
    }
    
    auto expect_monotonically_decreasing(
        auto &&range,
        auto &&projection,
        const bool strict,
        const std::optional<std::size_t> expected_range_size)
    {
        using ValueT = std::remove_cvref_t<
            decltype(projection(*range.begin()))
        >;
        std::optional<ValueT> max;
        ValueT accumulation { };
        std::size_t n_visited = 0;
        for(auto &&val : range)
        {
            const ValueT projected = projection(val);
            accumulation += projected;
            if(max.has_value())
            {
                // strictly increasing
                if(strict) EXPECT_GT(max.value(), projected);
                // allow remaining constant
                else EXPECT_GE(max.value(), projected);
            }
            max = projected;
            ++n_visited;
        }
        if(expected_range_size)
        {
            EXPECT_EQ(*expected_range_size, n_visited);
        }
        return accumulation;
    }
};
}
