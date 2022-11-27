#include <gtest/gtest.h>

#include <fmt/ostream.h>


#include <Usagi/Library/Container/BinaryHeap.hpp>
#include <Usagi/Modules/Algorithms/Optimization/Evolutionary/Genome/Permutation/CChromosomePermutation.hpp>
#include <Usagi/Modules/Algorithms/Statistics/RandomNumbers/ServiceRandomNumberGenerator.hpp>
#include <Usagi/Modules/Common/Logging/Logging.hpp>

#include "../../Projects/Research/LabScheduler/Methodology/Shared/ProcessorStatus.hpp"
#include "../../Projects/Research/LabScheduler/Methodology/Shared/ServiceTaskGraph.hpp"

using namespace usagi;
// using namespace research;

namespace 
{
constexpr std::size_t num_proc = 3;
constexpr std::size_t num_tasks = 7;
constexpr std::size_t num_subtasks = 2;

using TaskIndexT = SmallestCapableType<num_tasks>;
using SubtaskIndexT = SmallestCapableType<num_subtasks>;
using SeqIndexT = SmallestCapableType<num_tasks * num_subtasks>;
using ProcIndexT = SmallestCapableType<num_proc>;
using TimePointT = double;

using research::TaskGraph;
using research::ProcessorStatus;
using research::ServiceTaskGraph;

// track the availability status of tasks
struct TaskSchedulingStatus
{
    SubtaskIndexT num_scheduled_subtasks = 0;
    TaskIndexT num_unfinished_predecessors = -1;
    TimePointT ready_time = 0;
    TimePointT finish_time = -1;
};

struct ExampleTaskTraits
{
    TaskIndexT task_id = -1;
    SubtaskIndexT subtask_id = -1;
    const TaskGraph &graph;
    std::array<TaskSchedulingStatus, num_tasks> &statuses;

    // todo DurationT
    // todo should adjust according to processor profile
    TimePointT exec_time(ProcIndexT proc_index)
    {
        // todo return the real one by querying the task profile & proc profile
        // todo handle heterogeneity
        return 10;
    }

    TimePointT ready_time()
    {
        return statuses[task_id].ready_time;
    }

    // this has nothing to do with slot selection. it only does the actual
    // assignment
    auto negotiate_time_assignment(
        // auto &&out_schedule,
        auto proc_index,
        auto proc_idle_begin,
        auto proc_idle_end)
    {
        auto task_begin = std::max(proc_idle_begin, ready_time());
        // exec time adjusted by proc profile?
        auto task_end = task_begin + exec_time(proc_index);

        assert(task_end <= proc_idle_end);

        return std::tuple { task_begin, task_end };
    }

    void on_scheduled(
        auto &&enqueue_func,
        auto proc_index,
        auto task_begin,
        auto task_end)
    {
        auto &status = statuses[task_id];
        auto &num_scheduled_sub = status.num_scheduled_subtasks;
        assert(num_scheduled_sub < num_subtasks);

        LOG(info, "task[{}][{}] scheduled to proc={} time=[{}, {}]."
            " {} subtasks left to be scheduled.",
            task_id, subtask_id, proc_index, task_begin, task_end, 
            num_subtasks - num_scheduled_sub);

        status.finish_time = std::max(status.finish_time, task_end);

        LOG(info, "task[{}].finish_time updated: {}\n", task_id, status.finish_time);

        // we got one whole task scheduled
        if(++num_scheduled_sub == num_subtasks)
        {
            LOG(info, "all subtasks of task[{}] are scheduled. "
                "propagating ready state", task_id);

            // propagate ready state
            for(auto suc : graph.adjacent_vertices(task_id))
            {
                auto &suc_status = statuses[suc];
                suc_status.ready_time = std::max(
                    suc_status.ready_time,
                    status.finish_time
                );
                --suc_status.num_unfinished_predecessors;

                LOG(info, "descedent {} has {} unscheduled predecessors",
                    suc, suc_status.num_unfinished_predecessors);

                if(suc_status.num_unfinished_predecessors == 0)
                {
                    LOG(info, "descedent {} is enqueued for scheduling",
                        suc);
                    enqueue_func(suc);
                }
            }
        }
    }
};
}

TEST(SchedulingTest, ListSchedulerSubtasks)
{
    // prepare the input params and a dummy graph

    ServiceThreadLocalRNG rng;
    ServiceTaskGraph graph;
    ProcessorStatus schedule;
    schedule.reset(num_proc);

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

        graph.set_task_graph(tg);
    }

    // init task priorities
    CChromosomePermutation<num_subtasks * num_tasks> chromosome;
    chromosome.init_random(rng.get_service());

 
    // init in-degrees
    std::array<TaskSchedulingStatus, num_tasks> task_statuses;
    for(std::size_t i = 0; auto &&status : task_statuses)
    {
        status.num_unfinished_predecessors = graph.in_degrees()[i];
        ++i;
    }


    // const auto projection = [&](const TaskIndexT &seq_idx) {
    //     // const auto sequential = job_index.task * splittability(0) +
    //         // job_index.subtask;
    //     // return schedule_order.order[sequential];
    //     return chromosome.permutation[seq_idx];
    // };

    // // this type should be transparent to the scheduler
    // struct Workload
    // {
    //     TaskIndexT task_id = -1;
    //     SubtaskIndexT subtask_id = -1;
    // };
    //
    //
    // // todo use real exec time
    // auto task_exec_time = [](const Workload &) {
    //     return 10;
    // };
    //
    //
    // auto workload_from_seq_id = [&](SeqIndexT seq_id) {
    //     return Workload {
    //         .task_id = TaskIndexT(seq_id / num_tasks),
    //         .subtask_id = SubtaskIndexT(seq_id % num_tasks)
    //     };
    // };
    //
    // const auto projection = [&](const Workload &workload) {
    //     // const auto sequential = job_index.task * splittability(0) +
    //         // job_index.subtask;
    //     // return schedule_order.order[sequential];
    //     return chromosome.permutation[
    //         workload.task_id * num_subtasks + workload.subtask_id];
    // };

    const auto projection = [&](SeqIndexT seq_id) {
        // const auto sequential = job_index.task * splittability(0) +
        // job_index.subtask;
        // return schedule_order.order[sequential];
        return chromosome.permutation[seq_id];
    };

    // prepare priority queue of unscheduled tasks
    // this should be injected to the scheduler again it should be transparent

    // basically we are choosing a topological order defined by the
    // scheduling order in the chromosome.
    BinaryHeap<
        SeqIndexT,              // no concept of subtask here. it's transparent to the queue
        decltype(projection),   // read scheduling priorities from the chromosome
        std::less<>,            // compare scheduling order
        HeapElementTraitNoop    // no need to update key
    > queue(projection);

    const auto enqueue_all_sub = [&](TaskIndexT task_id) {
        // insert all subtasks
        const auto begin = task_id * num_subtasks;
        for(SubtaskIndexT i = 0; i < num_subtasks; ++i)
        {
            LOG(info, "enqueued task[{}][{}] seq={}", task_id, i, begin + i);
            queue.insert(begin + i);
        }
    };

    // enqueue all tasks without dependencies
    for(TaskIndexT i = 0; i < graph.num_tasks(); ++i)
    {
        if(graph.in_degrees()[i] == 0)
            enqueue_all_sub(i);
    }

    // schedule the beginning tasks
    // schedule_all_sub(0);

    auto scheduler_alloc_policy =
        // todo retrieve real work time
        // todo make sure the slot can contain the workload
    [&](ProcessorStatus &schedule_, auto &&task) {

        auto [proc_index, proc_idle_begin] = schedule_.earliest_available();

        return std::tuple {
            proc_index,
            proc_idle_begin,
            /*proc_idle_end*/
            std::numeric_limits<TimePointT>::max()
        };
    };


    // how to use a function to handle the impact of scheduling a specific task
    // on a specific time slot. (interference with co-scheduled tasks?
    // i think there is no need to make any decisions based on that information
    // (i mean let the GA to figure that out. just that, with SMT if coscheduling
    // one task affects other already scheduled tasks how should that be handled?
    // it may have propogational effects if the prec constraints are to be maintained
    // it might be complicated. so ignore it for now.

    // run the scheduler

    auto greedy_list_scheduler =
    []( auto &&task_queue,          // priority queue of unscheduled tasks
        auto &&enqueue_func,
        auto &&task_traits,         // converts element popped from pq to task info
        // todo this is really only a time slot manager
        auto &&out_schedule,        // output schedule
        auto &&proc_alloc_policy)   // picks empty slot on the schedule
        // auto &&on_task_scheduled)   // callback called when a task is scheduled
    {
        TimePointT makespan = 0;
        // process every unscheduled task
        while(!task_queue.empty())
        {
            // get the task profile
            auto task = task_traits(task_queue.top());
            task_queue.pop();

            // todo iterate over slot range? - should be the task of the policy
            auto [proc_index, proc_idle_begin, proc_idle_end] =
                proc_alloc_policy(out_schedule, task);
            // scheduler_alloc_policy(out_schedule, task);

            auto [task_begin, task_end] = task.negotiate_time_assignment(
                // out_schedule,
                proc_index,
                proc_idle_begin,
                proc_idle_end
            );

            // todo this might actually fail
            // todo handle task interference/SMT
            out_schedule.occupy(proc_index, task_begin, task_end);

            // the task trait is responsible for pushing any further tasks
            task.on_scheduled(enqueue_func, proc_index, task_begin, task_end);

            makespan = std::max(makespan, task_end);
        }
        return makespan;
    };

    auto task_traits = [&](SeqIndexT seq_idx) {
        LOG(info, "popped task[{}][{}] seq={}", 
            TaskIndexT(seq_idx / num_subtasks),
            SubtaskIndexT(seq_idx % num_subtasks),
            seq_idx
        );
        return ExampleTaskTraits {
            .task_id = TaskIndexT(seq_idx / num_subtasks),
            .subtask_id = SubtaskIndexT(seq_idx % num_subtasks),
            .graph = graph.task_graph(),
            .statuses = task_statuses
        };
    };

    const auto makespan = greedy_list_scheduler(
        queue,
        enqueue_all_sub,
        task_traits,
        schedule,
        scheduler_alloc_policy
    );
    LOG(info, "makespan={}", makespan);
}
