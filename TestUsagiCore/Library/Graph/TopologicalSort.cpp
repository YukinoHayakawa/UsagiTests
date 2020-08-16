#include <gtest/gtest.h>

#include <Usagi/Library/Graph/TopologicalSort.hpp>

#include "TestGraphAdjMatFixed.hpp"

TEST(GraphAlgorithms, TopologicalSortFixed)
{
    constexpr auto sort = topological_sort(test_graph());

    constexpr auto cmp = [&]() {
        // in reserve order
        auto order = std::array<int, 11> { 0, 6, 1, 3, 10, 4, 9, 7, 8, 2, 5 };
        for(auto i = 0; i < 11; ++i)
            if(order[i] != sort.stack[i]) return false;
        return true;
    }();

    static_assert(cmp);
}
