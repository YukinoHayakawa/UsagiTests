#define _SILENCE_CLANG_CONCEPTS_MESSAGE
#ifdef _DEBUG
#pragma comment(lib, "gtestd.lib")
#else
#pragma comment(lib, "gtest.lib")
#endif

#include <gtest/gtest.h>
#include <Usagi/Executor/detail/CpgValidation.hpp>
#include <Usagi/Library/Graph/TopologicalSort.hpp>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <array>

#include <Usagi/Executor/TaskGraph.hpp>

using namespace usagi;

constexpr SystemPrecedenceGraph<11> task_graph()
{
    SystemPrecedenceGraph<11> g;

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

TEST(TestExecutor, TaskGraphCycleDetection)
{
    static_assert(!cycle_test_false());
    static_assert(cycle_test_true());
}

TEST(TestExecutor, TaskGraphTransitiveReduction)
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

TEST(TestExecutor, TaskGraphTopologicalSort)
{
    constexpr auto sort = topological_sort(task_graph());

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

TEST(TestExecutor, ComponentGraphTest)
{
    using TaskGraph = TaskGraph<
        System0, System1, System2, System3, System4,
        System5
    >;

    // test cpg deduction

    constexpr auto cpg = []() {
        TaskGraph g;
        g.precede<System0, System1>();
        g.precede<System0, System2>();
        g.precede<System1, System3>();
        g.precede<System1, System4>();
        g.precede<System2, System5>();
        g.precede<System3, System5>();
        g.precede<System4, System5>();
        return g.component_precedence_graph<ComponentA>();
    }();

    constexpr auto compare = []() {
        TaskGraph::Graph cpg_cmp;
        cpg_cmp.add_edge(0, 1);
        cpg_cmp.add_edge(1, 4);
        cpg_cmp.add_edge(4, 5);
        cpg_cmp.system_traits[0] = ComponentAccess::READ;
        cpg_cmp.system_traits[1] = ComponentAccess::WRITE;
        cpg_cmp.system_traits[4] = ComponentAccess::WRITE;
        cpg_cmp.system_traits[5] = ComponentAccess::READ;
        return cpg_cmp;
    }();

     for(auto i = 0; i < 6; ++i)
         for(auto j = 0; j < 6; ++j)
             EXPECT_EQ(cpg.matrix[i][j], compare.matrix[i][j]);

    static_assert(cpg == compare);

    // auto check1 = cpg_validate(compare);
    constexpr auto check1 = cpg_validate(compare);
    static_assert(CHECK_TASK_GRAPH(check1));

    // test cwp uniqueness

    constexpr auto cpg_shortcut = []() {
        TaskGraph::Graph cpg;
        cpg.add_edge(0, 1);
        cpg.add_edge(1, 4);
        cpg.add_edge(4, 5);
        cpg.add_edge(0, 2);
        cpg.add_edge(2, 5);
        cpg.system_traits[0] = ComponentAccess::READ;
        cpg.system_traits[1] = ComponentAccess::WRITE;
        cpg.system_traits[2] = ComponentAccess::WRITE;
        cpg.system_traits[4] = ComponentAccess::WRITE;
        cpg.system_traits[5] = ComponentAccess::READ;
        return cpg;
    }();

    constexpr auto check2 = cpg_validate(cpg_shortcut);
    static_assert(check2.error_code == ErrorCode::CPG_SHORTCUT_WRITE_SYSTEM);
    static_assert(check2.info == 4);
}

constexpr SystemPrecedenceGraph<12> cpg_base()
{
    SystemPrecedenceGraph<12> g;

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

constexpr SystemPrecedenceGraph<12> cpg_shortcut_1()
{
    SystemPrecedenceGraph<12> g = cpg_base();
    g.add_edge(2, 3);
    g.system_traits[3] = ComponentAccess::READ;
    return g;
}

constexpr SystemPrecedenceGraph<12> cpg_shortcut_2()
{
    SystemPrecedenceGraph<12> g = cpg_base();
    g.add_edge(4, 7);
    return g;
}

constexpr SystemPrecedenceGraph<12> cpg_shortcut_3()
{
    SystemPrecedenceGraph<12> g = cpg_base();

    g.add_edge(7, 9);
    g.add_edge(9, 11);

    g.system_traits[9] = ComponentAccess::READ;
    g.system_traits[11] = ComponentAccess::WRITE;

    return g;
}

TEST(TestExecutor, ComponentGraphShortcut)
{
    {
        constexpr auto check = cpg_validate(cpg_base());
        static_assert(check.error_code == ErrorCode::SUCCEED);
    }
    {
        constexpr auto check = cpg_validate(cpg_shortcut_1());
        static_assert(check.error_code == ErrorCode::CPG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 10);
    }
    {
        constexpr auto check = cpg_validate(cpg_shortcut_2());
        static_assert(check.error_code == ErrorCode::CPG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 5);
    }
    {
        constexpr auto check = cpg_validate(cpg_shortcut_3());
        static_assert(check.error_code == ErrorCode::CPG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 11);
    }
}

/*
TEST(TestExecutor, TaskGraphTopologicalSortDyn)
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
