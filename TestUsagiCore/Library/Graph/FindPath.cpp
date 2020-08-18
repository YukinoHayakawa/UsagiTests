#include <gtest/gtest.h>

#include <Usagi/Library/Graph/AdjacencyList.hpp>
#include <Usagi/Library/Graph/FindPath.hpp>
#include <Usagi/Library/Graph/ExtractPath.hpp>

using namespace usagi::graph;

TEST(GraphAlgorithms, LongestPathDynamic)
{
    AdjacencyList g { 6 };

    g.add_edge(0, 1, 5);
    g.add_edge(0, 2, 3);
    g.add_edge(1, 3, 6);
    g.add_edge(1, 2, 2);
    g.add_edge(2, 4, 4);
    g.add_edge(2, 5, 2);
    g.add_edge(2, 3, 7);
    g.add_edge(3, 5, 1);
    g.add_edge(3, 4, -1);
    g.add_edge(4, 5, -2);

    const auto [prev, dist] = longest_path_dag(g, 1);
    EXPECT_EQ(dist[5], 10);

    auto path = path_to_stack<AdjacencyList>(prev, 1, 5);

    EXPECT_EQ(path.top(), 1);
    path.pop();
    EXPECT_EQ(path.top(), 2);
    path.pop();
    EXPECT_EQ(path.top(), 3);
    path.pop();
    EXPECT_EQ(path.top(), 5);
    path.pop();
    EXPECT_TRUE(path.empty());

    const auto path_vec = path_to_array<AdjacencyList>(prev, 1, 5);

    EXPECT_EQ(path_vec.size(), 4);
    EXPECT_EQ(path_vec[0], 1);
    EXPECT_EQ(path_vec[1], 2);
    EXPECT_EQ(path_vec[2], 3);
    EXPECT_EQ(path_vec[3], 5);
}

TEST(GraphAlgorithms, ShortestPathDynamic)
{
    AdjacencyList g { 6 };

    g.add_edge(0, 1, 5);
    g.add_edge(0, 2, 3);
    g.add_edge(1, 3, 6);
    g.add_edge(1, 2, 2);
    g.add_edge(2, 4, 4);
    g.add_edge(2, 5, 2);
    g.add_edge(2, 3, 7);
    g.add_edge(3, 5, 1);
    g.add_edge(3, 4, -1);
    g.add_edge(4, 5, -2);

    const auto [prev, dist] = shortest_path_dag(g, 1);
    EXPECT_EQ(dist[5], 3);

    auto path = path_to_stack<AdjacencyList>(prev, 1, 5);

    EXPECT_EQ(path.top(), 1);
    path.pop();
    EXPECT_EQ(path.top(), 3);
    path.pop();
    EXPECT_EQ(path.top(), 4);
    path.pop();
    EXPECT_EQ(path.top(), 5);
    path.pop();
    EXPECT_TRUE(path.empty());

    const auto path_vec = path_to_array<AdjacencyList>(prev, 1, 5);

    EXPECT_EQ(path_vec.size(), 4);
    EXPECT_EQ(path_vec[0], 1);
    EXPECT_EQ(path_vec[1], 3);
    EXPECT_EQ(path_vec[2], 4);
    EXPECT_EQ(path_vec[3], 5);
}
