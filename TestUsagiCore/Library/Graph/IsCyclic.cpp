#include <gtest/gtest.h>

#include <Usagi/Library/Graph/IsCyclic.hpp>

#include "TestGraphFixed.hpp"

constexpr bool cycle_test_false()
{
    constexpr auto g = test_graph();
    return is_cyclic(g);
}

constexpr bool cycle_test_true()
{
    auto g = test_graph();

    // creates a cycle
    g.add_edge(6, 7);

    return is_cyclic(g);
}

TEST(GraphAlgorithms, IsCyclicFixed)
{
    static_assert(!cycle_test_false());
    static_assert(cycle_test_true());
}
