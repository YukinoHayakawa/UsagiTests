#include <gtest/gtest.h>

#include <Usagi/Modules/Algorithms/Optimization/Scheduling/Constraints/ScheduleConstraintGraphSimple.hpp>
#include <Usagi/Modules/Common/Logging/Logging.hpp>

#include "../../Projects/Research/LabScheduler/Methodology/Shared/ServiceTaskGraph.hpp"

using namespace usagi;
using namespace research;

TEST(SchedulerTest, SimpleConstraintGraphTest)
{
    const auto num_proc = 4;
    const auto num_subtasks = 2;

    ServiceTaskGraph task_graph;
    {
        TaskGraph tg;
        tg.resize(7);
        tg.add_edge(0, 1);
        tg.add_edge(1, 2);
        tg.add_edge(1, 3);
        tg.add_edge(2, 4);
        tg.add_edge(4, 5);
        tg.add_edge(3, 5);
        tg.add_edge(5, 6);

        task_graph.set_task_graph(tg);
    }

    ScheduleConstraintGraphSimple constraint_graph;
    // using VertexIndexT = ScheduleConstraintGraphSimple::VertexIndexT;

    auto [root_idx, root_ref] = constraint_graph.add_vertex<ScheduleNodeRoot>();

    // init processors

    LOG(error, "creating {} processors", num_proc);

    for(auto i = 0; i < num_proc; ++i)
    {
        auto [index, proc] = constraint_graph.add_vertex<ScheduleNodeProcessorReady>();
        proc.proc_id = i;
        proc.ready_time = 0;

        constraint_graph.add_edge<ScheduleNodeRoot, ScheduleNodeProcessorReady>(root_idx, index);
    }

    // init task & barriers

    LOG(error, "creating subtasks & barriers");

    std::map<int, ScheduleConstraintGraphSimple::VertexIndexT> task_begin_indices;
    std::map<int, ScheduleConstraintGraphSimple::VertexIndexT> task_end_indices;

    for(auto i = 0; i < task_graph.num_tasks(); ++i)
    {
        // add the beginning barrier
        auto [begin_idx, begin_ref] = constraint_graph.add_vertex<ScheduleNodeBarrier>();
        begin_ref.num_waiting_input = task_graph.in_degrees()[i];
        task_begin_indices[i] = begin_idx;

        // add the finishing barrier
        auto [end_idx, end_ref] = constraint_graph.add_vertex<ScheduleNodeBarrier>();
        end_ref.num_waiting_input = num_subtasks;
        task_end_indices[i] = end_idx;

        // add subtasks
        for(auto j = 0; j < num_subtasks; ++j)
        {
            auto [sub_idx, sub_ref] = constraint_graph.add_vertex<ScheduleNodeExec>();
            static_assert(std::is_reference_v<decltype(sub_ref)>);
            sub_ref.task_id = i;
            sub_ref.subtask_id = j;
            sub_ref.exec_time = 12;
            constraint_graph.add_edge<ScheduleNodeBarrier, ScheduleNodeExec>(begin_idx, sub_idx);
            constraint_graph.add_edge<ScheduleNodeExec, ScheduleNodeBarrier>(sub_idx, end_idx);
        }
    }

    LOG(error, "adding precedence constraints");

    // add precedence constraints
    for(auto i = 0; i < task_graph.num_tasks(); ++i)
    {
        for(auto &&out : task_graph.task_graph().adjacent_vertices(i))
        {
            constraint_graph.add_edge<ScheduleNodeBarrier, ScheduleNodeBarrier>(
                task_end_indices.at(i),
                task_begin_indices.at(out)
            );
        }
    }

    // push initial task
    constraint_graph.add_edge<ScheduleNodeRoot, ScheduleNodeBarrier>(
        root_idx, 
        task_begin_indices[task_graph.source_task_id()]
    );

    // VertexIndexT proc_idx = -1;

    auto alloc_policy_earliest_idle = [&] {
        auto range = constraint_graph.proc_ready_nodes();
        auto iter = std::ranges::min_element(
            range,
            std::ranges::less { },
            [](auto &&p) { return std::get<1>(p); }
        );
        return std::pair { std::move(range), std::move(iter) };
    };
    
    // perform list scheduling
    while(!constraint_graph.task_queue.empty())
    {
        auto task = constraint_graph.task_queue.top();
        constraint_graph.task_queue.pop();

        auto [proc_range, proc_iter] = alloc_policy_earliest_idle();
        assert(proc_iter != proc_range.end());
        /*auto &exec = */constraint_graph.add_edge<
            ScheduleNodeProcessorReady,
            ScheduleNodeExec
        >(std::get<0>(*proc_iter), task);
    }

    LOG(error, "makespan: {}",constraint_graph.vertex<ScheduleNodeBarrier>(
        task_end_indices[task_graph.sink_task_id()]).finish_time);

    // push all ready tasks into the queue


    (void)0;

    // constraint_graph.add_edge<ScheduleNodeProcessorReady, ScheduleNodeExec>(1, 7);

    (void)0;

    // constraint_graph.add_edge<ScheduleNodeProcessorReady, ScheduleNodeExec>(2, 8);

    (void)0;

}
