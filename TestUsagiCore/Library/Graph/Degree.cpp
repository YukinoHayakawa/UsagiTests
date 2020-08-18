#include <gtest/gtest.h>

#include <Usagi/Library/Graph/Degree.hpp>

#include "TestGraphAdjMatFixed.hpp"

TEST(GraphAlgorithms, InDegrees)
{
    constexpr auto g = test_graph();
    constexpr auto in_deg = in_degree(g);

    static_assert(in_deg[0] == 0);
    static_assert(in_deg[1] == 1);
    static_assert(in_deg[2] == 0);
    static_assert(in_deg[3] == 1);
    static_assert(in_deg[4] == 1);
    static_assert(in_deg[5] == 0);
    static_assert(in_deg[6] == 4);
    static_assert(in_deg[7] == 1);
    static_assert(in_deg[8] == 1);
    static_assert(in_deg[9] == 2);
    static_assert(in_deg[10] == 2);
}

TEST(GraphAlgorithms, OutDegrees)
{
    constexpr auto g = test_graph();
    constexpr auto out_deg = out_degree(g);

    static_assert(out_deg[0] == 0);
    static_assert(out_deg[1] == 1);
    static_assert(out_deg[2] == 3);
    static_assert(out_deg[3] == 1);
    static_assert(out_deg[4] == 2);
    static_assert(out_deg[5] == 0);
    static_assert(out_deg[6] == 0);
    static_assert(out_deg[7] == 3);
    static_assert(out_deg[8] == 1);
    static_assert(out_deg[9] == 1);
    static_assert(out_deg[10] == 1);
}
