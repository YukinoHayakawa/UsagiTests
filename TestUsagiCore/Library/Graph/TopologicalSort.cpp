#include <gtest/gtest.h>

#include <Usagi/Library/Graph/TopologicalSortLexical.hpp>

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

TEST(GraphAlgorithms, TopologicalSortFixedLexical)
{
    constexpr auto sort = topological_sort_lexical_smallest(graph2());

    constexpr auto cmp = [&]() {
        auto order = std::array<int, 6> { 0, 1, 2, 3, 4, 5 };
        for(auto i = 0; i < 6; ++i)
            if(order[i] != sort[i]) return false;
        return true;
    }();

    static_assert(cmp);
}

TEST(GraphAlgorithms, TopologicalSortFixedLexical2)
{
    constexpr auto sort = topological_sort_lexical_smallest(graph3());

    constexpr auto cmp = [&]() {
        auto order = std::array<int, 6> { 4, 5, 0, 2, 3, 1 };
        for(auto i = 0; i < 6; ++i)
            if(order[i] != sort[i]) return false;
        return true;
    }();

    static_assert(cmp);
}
