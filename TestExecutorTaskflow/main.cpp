﻿#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <Usagi/Module/Common/ExecutorTaskflow/Graph.hpp>
#include <Usagi/Module/Common/ExecutorTaskflow/TaskGraph.hpp>

using namespace usagi;

constexpr AdjacencyMatrixGraph<11> task_graph()
{
    AdjacencyMatrixGraph<11> g;

    g.add_edge(2, 4);
    g.add_edge(2, 7);
    g.add_edge(2, 8);
    g.add_edge(4, 3);
    g.add_edge(4, 10);
    g.add_edge(7, 10);
    g.add_edge(7, 1);
    g.add_edge(7, 9);
    g.add_edge(8, 9);
    g.add_edge(3, 6);
    g.add_edge(10, 6);
    g.add_edge(1, 6);
    g.add_edge(9, 6);

    return g;
}

constexpr bool cycle_test_false()
{
    constexpr auto g = task_graph();
    return g.is_cyclic();
}

constexpr bool cycle_test_true()
{
    auto g = task_graph();

    // creates a cycle
    g.add_edge(6, 7);

    return g.is_cyclic();
}

TEST(TestExecutorTaskflow, TaskGraphCycleDetection)
{
    static_assert(!cycle_test_false());
    static_assert(cycle_test_true());
}

TEST(TestExecutorTaskflow, TaskGraphTransitiveReduction)
{
    constexpr auto g = task_graph();
    constexpr auto g2 = []() {
        auto g2 = task_graph();

        g2.add_edge(2, 1);
        g2.add_edge(2, 3);
        g2.add_edge(2, 6);
        g2.add_edge(2, 9);
        g2.add_edge(2, 10);
        g2.add_edge(4, 6);
        g2.add_edge(7, 6);
        g2.add_edge(8, 6);

        g2.transitive_reduce();

        return g2;
    }();

    static_assert(g == g2);
}

TEST(TestExecutorTaskflow, TaskGraphTopologicalSort)
{
    Graph g, rg;

    g.add_edge(2, 4);
    g.add_edge(2, 7);
    g.add_edge(2, 8);
    g.add_edge(4, 3);
    g.add_edge(4, 10);
    g.add_edge(7, 10);
    g.add_edge(7, 1);
    g.add_edge(7, 9);
    g.add_edge(8, 9);
    g.add_edge(3, 6);
    g.add_edge(10, 6);
    g.add_edge(1, 6);
    g.add_edge(9, 6);

    rg.add_edge(4, 2);
    rg.add_edge(7, 2);
    rg.add_edge(8, 2);
    rg.add_edge(3, 4);
    rg.add_edge(10, 4);
    rg.add_edge(10, 7);
    rg.add_edge(1, 7);
    rg.add_edge(9, 7);
    rg.add_edge(9, 8);
    rg.add_edge(6, 3);
    rg.add_edge(6, 10);
    rg.add_edge(6, 1);
    rg.add_edge(6, 9);

    TopologicalSort ts { g };
    TopologicalSort rts { rg };

    ts.sort(2);
    for(int i = 1; i < 11; ++i)
    {
        std::cout << "[" << i << "] ";
        rts.sort(i);
    }
}
