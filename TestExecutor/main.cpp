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

constexpr GraphAdjacencyMatrix<11> task_graph()
{
    GraphAdjacencyMatrix<11> g;

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

    constexpr auto g = []() {
        TaskGraph g;
        g.precede<System0, System1>();
        g.precede<System0, System2>();
        g.precede<System1, System3>();
        g.precede<System1, System4>();
        g.precede<System2, System5>();
        g.precede<System3, System5>();
        g.precede<System4, System5>();
        return g;
    }();
    constexpr auto cpg = g.component_precedence_graph<ComponentA>();
    constexpr auto sat = g.system_access_traits<ComponentA>();

    constexpr auto compare = []() {
        TaskGraph::GraphT cpg_cmp;
        cpg_cmp.add_edge(0, 1);
        cpg_cmp.add_edge(1, 4);
        cpg_cmp.add_edge(4, 5);
        return cpg_cmp;
    }();

     for(auto i = 0; i < 6; ++i)
         for(auto j = 0; j < 6; ++j)
             EXPECT_EQ(cpg.matrix[i][j], compare.matrix[i][j]);

    static_assert(cpg == compare);

    // auto check1 = cpg_validate(compare);
    constexpr auto check1 = cpg_validate(compare, sat);
    static_assert(CHECK_TASK_GRAPH(check1));

    // test cwp uniqueness

    constexpr auto cpg_shortcut = []() {
        TaskGraph::GraphT cpg;
        cpg.add_edge(0, 1);
        cpg.add_edge(1, 4);
        cpg.add_edge(4, 5);
        cpg.add_edge(0, 2);
        cpg.add_edge(2, 5);
        return cpg;
    }();

    constexpr auto sat_shortcut = []()
    {
        SystemAccessTraits<6> traits { };
        traits[0] = ComponentAccess::READ;
        traits[1] = ComponentAccess::WRITE;
        traits[2] = ComponentAccess::WRITE;
        traits[4] = ComponentAccess::WRITE;
        traits[5] = ComponentAccess::READ;
        return traits;
    }();

    constexpr auto check2 = cpg_validate(cpg_shortcut, sat_shortcut);
    static_assert(check2.error_code == ErrorCode::CPG_SHORTCUT_WRITE_SYSTEM);
    static_assert(check2.info == 4);
}

constexpr GraphAdjacencyMatrix<12> cpg_base()
{
    GraphAdjacencyMatrix<12> g;

    g.add_edge(0, 2);
    g.add_edge(1, 2);
    g.add_edge(2, 4);
    g.add_edge(5, 6);
    g.add_edge(4, 5);
    g.add_edge(6, 7);
    g.add_edge(7, 8);
    g.add_edge(8, 10);

    return g;
}

constexpr SystemAccessTraits<12> sat()
{
    SystemAccessTraits<12> traits { };

    traits[0] = ComponentAccess::READ;
    traits[1] = ComponentAccess::READ;
    traits[4] = ComponentAccess::READ;
    traits[6] = ComponentAccess::READ;
    traits[8] = ComponentAccess::READ;
    traits[2] = ComponentAccess::WRITE;
    traits[5] = ComponentAccess::WRITE;
    traits[7] = ComponentAccess::WRITE;
    traits[10] = ComponentAccess::WRITE;

    return traits;
}

constexpr SystemAccessTraits<12> sat_1()
{
    auto traits = sat();
    traits[3] = ComponentAccess::READ;
    return traits;
}

constexpr GraphAdjacencyMatrix<12> cpg_shortcut_1()
{
    GraphAdjacencyMatrix<12> g = cpg_base();
    g.add_edge(2, 3);
    return g;
}

constexpr GraphAdjacencyMatrix<12> cpg_shortcut_2()
{
    GraphAdjacencyMatrix<12> g = cpg_base();
    g.add_edge(4, 7);
    return g;
}

constexpr SystemAccessTraits<12> sat_3()
{
    auto traits = sat();
    traits[9] = ComponentAccess::READ;
    traits[11] = ComponentAccess::WRITE;
    return traits;
}

constexpr GraphAdjacencyMatrix<12> cpg_shortcut_3()
{
    GraphAdjacencyMatrix<12> g = cpg_base();

    g.add_edge(7, 9);
    g.add_edge(9, 11);

    return g;
}

TEST(TestExecutor, ComponentGraphShortcut)
{
    {
        constexpr auto check = cpg_validate(cpg_base(), sat());
        static_assert(check.error_code == ErrorCode::SUCCEED);
    }
    {
        constexpr auto check = cpg_validate(cpg_shortcut_1(), sat_1());
        static_assert(check.error_code == ErrorCode::CPG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 10);
    }
    {
        constexpr auto check = cpg_validate(cpg_shortcut_2(), sat());
        static_assert(check.error_code == ErrorCode::CPG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 5);
    }
    {
        constexpr auto check = cpg_validate(cpg_shortcut_3(), sat_3());
        static_assert(check.error_code == ErrorCode::CPG_SHORTCUT_WRITE_SYSTEM);
        static_assert(check.info == 11);
    }
}

namespace
{
struct Ca {};
struct Cb {};

struct Sa
{
    using ReadAccess = ComponentFilter<>;
    using WriteAccess = ComponentFilter<Ca>;
};
struct Sb
{
    using ReadAccess = ComponentFilter<Ca>;
    using WriteAccess = ComponentFilter<>;
};
struct Sc
{
    using ReadAccess = ComponentFilter<>;
    using WriteAccess = ComponentFilter<Ca>;
};
struct Sd
{
    using ReadAccess = ComponentFilter<Ca, Cb>;
    using WriteAccess = ComponentFilter<>;
};
struct Se
{
    using ReadAccess = ComponentFilter<>;
    using WriteAccess = ComponentFilter<Cb>;
};
struct Sf
{
    using ReadAccess = ComponentFilter<Cb>;
    using WriteAccess = ComponentFilter<>;
};
struct Sg
{
    using ReadAccess = ComponentFilter<>;
    using WriteAccess = ComponentFilter<Cb>;
};
struct Sh
{
    using ReadAccess = ComponentFilter<Ca>;
    using WriteAccess = ComponentFilter<>;
};
}

TEST(TestExecutor, ReducedTaskGraphTest)
{
    // using TG = TaskGraph<Sa, Sb, Sc, Sd, Se, Sf, Sg, Sh>;
    using TG = TaskGraph<Sa, Sb, Sc, Sd, Se, Sf, Sg>;
    //                   0   1   2   3   4   5   6

    constexpr auto rtg = []()
    {
        TG tg;

        tg.precede<Sa, Sb>();
        tg.precede<Sb, Sc>();
        tg.precede<Sc, Sd>();
        tg.precede<Sa, Sc>();
        tg.precede<Sa, Sf>();
        tg.precede<Se, Sf>();
        tg.precede<Sf, Sg>();
        tg.precede<Sg, Sd>();
        tg.precede<Sa, Sb>();
        tg.precede<Sa, Sb>();
        // tg.precede<Sa, Sh>();
        // tg.precede<Sh, Sc>();

        return tg.reduced_task_graph();
    }();

    constexpr auto cmp = []()
    {
        TG::GraphT g;
        g.add_edge(0, 1);
        g.add_edge(1, 2);
        g.add_edge(2, 3);
        g.add_edge(4, 5);
        g.add_edge(5, 6);
        g.add_edge(6, 3);
        return g;
    }();

    static_assert(rtg == cmp);
}
