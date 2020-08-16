#include <gtest/gtest.h>

#include <Usagi/Library/Graph/AdjacencyList.hpp>
#include <Usagi/Library/Graph/LongestPath.hpp>
#include <Usagi/Library/Graph/ExtractPath.hpp>

using namespace usagi::graph;

TEST(GraphAlgorithms, LongestPathAdjListDyn)
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

    const auto prev = longest_path_dag(g, 1);
    auto path = extract_path<AdjacencyList>(prev, 1, 5);

    EXPECT_EQ(path.top(), 1);
    path.pop();
    EXPECT_EQ(path.top(), 2);
    path.pop();
    EXPECT_EQ(path.top(), 3);
    path.pop();
    EXPECT_EQ(path.top(), 5);
    path.pop();
    EXPECT_TRUE(path.empty());
}
