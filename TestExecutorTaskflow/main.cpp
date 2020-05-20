#define _SILENCE_CLANG_CONCEPTS_MESSAGE


#include <gtest/gtest.h>

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
    constexpr auto sort = task_graph().topological_sort();

    constexpr auto cmp = [&]() {
        // in reserve order
        auto order = std::array<int, 11> { 0, 6, 1, 3, 10, 4, 9, 7, 8, 2, 5 };
        for(auto i = 0; i < 11; ++i)
            if(order[i] != sort.stack[i]) return false;
        return true;
    }();

    static_assert(cmp);
}

struct ComponentA
{
};

struct System0
{
    using ReadAccess = ComponentFilter<ComponentA>;
};

struct System1
{
    using WriteAccess = ComponentFilter<ComponentA>;
};

struct System2
{
};

struct System3
{
};

struct System4
{
    using WriteAccess = ComponentFilter<ComponentA>;
};

struct System5
{
    using ReadAccess = ComponentFilter<ComponentA>;
};

#define CHECK_TASK_GRAPH(code) \
    TaskGraphErrorCodeCheck<code.error_code, code.info>::value \
/**/

TEST(TestExecutorTaskflow, ComponentGraphTest)
{
    using TaskGraph = TaskGraph<
        System0, System1, System2, System3, System4,
        System5
    >;

    // test cdg deduction

    constexpr auto cdg = []() {
        TaskGraph g;
        g.precede<System0, System1>();
        g.precede<System0, System2>();
        g.precede<System1, System3>();
        g.precede<System1, System4>();
        g.precede<System2, System5>();
        g.precede<System3, System5>();
        g.precede<System4, System5>();
        return g.component_dependency_graph<ComponentA>();
    }();

    constexpr auto compare = []() {
        TaskGraph::Graph cdg_cmp;
        cdg_cmp.add_edge(0, 1);
        cdg_cmp.add_edge(1, 4);
        cdg_cmp.add_edge(4, 5);
        cdg_cmp.system_traits[0] = ComponentAccess::READ;
        cdg_cmp.system_traits[1] = ComponentAccess::WRITE;
        cdg_cmp.system_traits[4] = ComponentAccess::WRITE;
        cdg_cmp.system_traits[5] = ComponentAccess::READ;
        return cdg_cmp;
    }();

     for(auto i = 0; i < 6; ++i)
         for(auto j = 0; j < 6; ++j)
             EXPECT_EQ(cdg.matrix[i][j], compare.matrix[i][j]);

    static_assert(cdg == compare);

    // auto check1 = cdg_validate(compare);
    constexpr auto check1 = cdg_validate(compare);
    static_assert(CHECK_TASK_GRAPH(check1));

    // test cwp uniqueness

    constexpr auto cdg_shortcut = []() {
        TaskGraph::Graph cdg;
        cdg.add_edge(0, 1);
        cdg.add_edge(1, 4);
        cdg.add_edge(4, 5);
        cdg.add_edge(0, 2);
        cdg.add_edge(2, 5);
        cdg.system_traits[0] = ComponentAccess::READ;
        cdg.system_traits[1] = ComponentAccess::WRITE;
        cdg.system_traits[2] = ComponentAccess::WRITE;
        cdg.system_traits[4] = ComponentAccess::WRITE;
        cdg.system_traits[5] = ComponentAccess::READ;
        return cdg;
    }();

    constexpr auto check2 = cdg_validate(cdg_shortcut);
    static_assert(check2.error_code == ErrorCode::CDG_SHORTCUT_WRITE_SYSTEM);
    static_assert(check2.info == 4);
}

constexpr AdjacencyMatrixGraph<12> cdg_base()
{
    AdjacencyMatrixGraph<12> g;

    g.add_edge(0, 2);
    g.add_edge(1, 2);
    g.add_edge(2, 4);
    g.add_edge(5, 6);
    g.add_edge(4, 5);
    g.add_edge(6, 7);
    g.add_edge(7, 8);
    g.add_edge(8, 10);

    g.system_traits[0] = ComponentAccess::READ;
    g.system_traits[1] = ComponentAccess::READ;
    g.system_traits[4] = ComponentAccess::READ;
    g.system_traits[6] = ComponentAccess::READ;
    g.system_traits[8] = ComponentAccess::READ;
    g.system_traits[2] = ComponentAccess::WRITE;
    g.system_traits[5] = ComponentAccess::WRITE;
    g.system_traits[7] = ComponentAccess::WRITE;
    g.system_traits[10] = ComponentAccess::WRITE;

    return g;
}

constexpr AdjacencyMatrixGraph<12> cdg_shortcut_1()
{
    AdjacencyMatrixGraph<12> g = cdg_base();
    g.add_edge(2, 3);
    g.system_traits[3] = ComponentAccess::READ;
    return g;
}

constexpr AdjacencyMatrixGraph<12> cdg_shortcut_2()
{
    AdjacencyMatrixGraph<12> g = cdg_base();
    g.add_edge(4, 7);
    return g;
}

constexpr AdjacencyMatrixGraph<12> cdg_shortcut_3()
{
    AdjacencyMatrixGraph<12> g = cdg_base();

    g.add_edge(7, 9);
    g.add_edge(9, 11);

    g.system_traits[9] = ComponentAccess::READ;
    g.system_traits[11] = ComponentAccess::WRITE;

    return g;
}

TEST(TestExecutorTaskflow, ComponentGraphShortcut)
{
    {
        constexpr auto check = cdg_validate(cdg_base());
        static_assert(check.error_code == ErrorCode::SUCCEED);
    }
    {
        constexpr auto check = cdg_validate(cdg_shortcut_1());
        static_assert(check.error_code == ErrorCode::CDG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 10);
    }
    {
        constexpr auto check = cdg_validate(cdg_shortcut_2());
        static_assert(check.error_code == ErrorCode::CDG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 5);
    }
    {
        constexpr auto check = cdg_validate(cdg_shortcut_3());
        static_assert(check.error_code == ErrorCode::CDG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 11);
    }
}

/*
TEST(TestExecutorTaskflow, TaskGraphTopologicalSortDyn)
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
*/
