#include <gtest/gtest.h>

#include <Usagi/Library/Graph/TransitiveReduce.hpp>

#include "TestGraphAdjMatFixed.hpp"

TEST(GraphAlgorithms, TransitiveReductionFixed)
{
    constexpr auto g = test_graph();
    constexpr auto g2 = []() {
        auto g2 = test_graph();

        g2.add_edge(2, 1);
        g2.add_edge(2, 3);
        g2.add_edge(2, 6);
        g2.add_edge(2, 9);
        g2.add_edge(2, 10);
        g2.add_edge(4, 6);
        g2.add_edge(7, 6);
        g2.add_edge(8, 6);

        transitive_reduce(g2);

        return g2;
    }();

    static_assert(g == g2);
}
