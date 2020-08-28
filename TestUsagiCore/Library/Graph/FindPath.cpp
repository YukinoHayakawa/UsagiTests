#include <gtest/gtest.h>

#include <Usagi/Library/Graph/AdjacencyList.hpp>
#include <Usagi/Library/Graph/AdjacencyMatrixFixed.hpp>
#include <Usagi/Library/Graph/FindPath.hpp>
#include <Usagi/Library/Graph/ExtractPath.hpp>
#include <Usagi/Library/Graph/Traversal.hpp>

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

    const auto [prev, dist, _] = longest_path_dag(g, 1);
    EXPECT_EQ(dist[5], 10);

    /*
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
    */

    const auto path_vec = path_to_array<AdjacencyList<>>(prev, 1, 5);

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

    const auto [prev, dist, _] = shortest_path_dag(g, 1);
    EXPECT_EQ(dist[5], 3);

    /*
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
    */

    const auto path_vec = path_to_array<AdjacencyList<>>(prev, 1, 5);

    EXPECT_EQ(path_vec.size(), 4);
    EXPECT_EQ(path_vec[0], 1);
    EXPECT_EQ(path_vec[1], 3);
    EXPECT_EQ(path_vec[2], 4);
    EXPECT_EQ(path_vec[3], 5);
}

auto test_graph_all_longest_paths()
{
    AdjacencyList g(11);

    g.add_edge(0, 1, 1);
    g.add_edge(0, 2, 1);
    g.add_edge(0, 3, 1);
    g.add_edge(1, 4, 1);
    g.add_edge(1, 5, 1);
    g.add_edge(2, 4, 1);
    g.add_edge(2, 5, 1);
    g.add_edge(2, 6, 1);
    g.add_edge(3, 5, 1);
    g.add_edge(3, 6, 1);
    g.add_edge(4, 7, 1);
    g.add_edge(5, 7, 1);
    g.add_edge(5, 8, 1);
    g.add_edge(6, 7, 1);
    g.add_edge(6, 8, 1);
    g.add_edge(7, 9, 1);
    g.add_edge(8, 9, 1);

    return g;
}

TEST(GraphAlgorithms, VerticesOnLongestPathsDynamic)
{
    auto g = test_graph_all_longest_paths();
    AdjacencyList out(g.num_vertices());

    auto [prev, dist, _] = longest_path_all_dag(g, out, 0);

    auto [vs, __] = dfs(out, 9);
    std::sort(vs.begin(), vs.end());

    EXPECT_EQ(vs.size(), 10);
    EXPECT_EQ(vs[0], 0);
    EXPECT_EQ(vs[1], 1);
    EXPECT_EQ(vs[2], 2);
    EXPECT_EQ(vs[3], 3);
    EXPECT_EQ(vs[4], 4);
    EXPECT_EQ(vs[5], 5);
    EXPECT_EQ(vs[6], 6);
    EXPECT_EQ(vs[7], 7);
    EXPECT_EQ(vs[8], 8);
    EXPECT_EQ(vs[9], 9);
}
