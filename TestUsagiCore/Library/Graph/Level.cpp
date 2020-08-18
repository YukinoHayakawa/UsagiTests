#include <gtest/gtest.h>

#include <Usagi/Library/Graph/Level.hpp>

#include "TestGraphAdjMatFixed.hpp"

using namespace usagi::graph;

TEST(GraphAlgorithms, Level)
{
    constexpr auto level1 = level(graph2());

    static_assert(level1[0] == 0);
    static_assert(level1[1] == 1);
    static_assert(level1[2] == 2);
    static_assert(level1[3] == 3);
    static_assert(level1[4] == 3);
    static_assert(level1[5] == 3);

    constexpr auto level2 = level(graph3());

    static_assert(level2[0] == 1);
    static_assert(level2[1] == 3);
    static_assert(level2[2] == 1);
    static_assert(level2[3] == 2);
    static_assert(level2[4] == 0);
    static_assert(level2[5] == 0);
}
