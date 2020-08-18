#include <gtest/gtest.h>

#include <Usagi/Library/Graph/TopologicalSortLexical.hpp>

#include "TestGraphFixed.hpp"

TEST(GraphAlgorithms, TopologicalSortFixed)
{
    constexpr auto sort = topological_sort(test_graph());
    constexpr std::array<int, 11> order { 5, 2, 8, 7, 9, 4, 10, 3, 1, 6, 0 };
    static_assert(sort == order);
}

TEST(GraphAlgorithms, TopologicalSortFixedLexical)
{
    constexpr auto sort = topological_sort_lexical_smallest(graph2());
    constexpr std::array<int, 6> order { 0, 1, 2, 3, 4, 5 };
    static_assert(sort == order);
}

TEST(GraphAlgorithms, TopologicalSortFixedLexical2)
{
    constexpr auto sort = topological_sort_lexical_smallest(graph3());
    constexpr std::array<int, 6> order { 4, 5, 0, 2, 3, 1 };
    static_assert(sort == order);
}
