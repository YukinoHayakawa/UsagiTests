# Testing

### Task Graph Executive: Failure Mode Matrix

| Failure Mode | Topological Cause | Executive Resolution Strategy | Status |
| :--- | :--- | :--- | :--- |
| **Infinite Re-entrancy** | A system queues structural mutations (e.g., spawns) but fails to consume the triggering condition, creating an unbounded Petri net. | The Executive tracks cycle depth. Upon breaching `MAX_RE_ENTRIES`, it aborts the frame and throws a fatal SEH/Exception. | Verified |
| **Isolated Node Exception** | A system's internal logic encounters an unhandled exception (e.g., Vulkan device loss, div-by-zero). | The Executive catches the exception, marks the node as `FAILED`, and allows independent parallel branches to continue execution. | Verified |
| **Cascading Dependent Abortion** | System A crashes. System B requires Read access to data written by System A. | If B runs, it reads corrupted state. The Executive mathematically infers the dependency edge and recursively aborts System B and all downstream nodes. | Verified |
| **Undeclared Mutation (Access Violation)** | A system attempts to `destroy` an entity but failed to declare `IntentDelete` in its `EntityQuery`. | Caught at compile-time via `DatabaseAccess` proxy using C++26 reflection (`static_assert`). Zero runtime cost. | Verified |
| **Overestimation Starvation** | JIT blast radius expands to cover the entire sparse matrix due to dense relational graphs. | Not a crash. Graph degrades to single-threaded sequential execution. Monitored via external `RealtimeProfilerService`. | Handled externally |

### Task Graph Executive: Exhaustive Failure & Edge Case Matrix

| Category | Scenario | Executive Resolution Strategy | Test Coverage |
| :--- | :--- | :--- | :--- |
| **Re-entrancy Limits** | **Infinite Loop Generation:** A System spawns new entities but fails to consume the condition token. | `re_entry_counter` breaches `MAX_RE_ENTRIES`. Executive aborts frame with a fatal `std::runtime_error`. | `Failure_InfiniteReEntrancy_ThrowsException` |
| **Re-entrancy Limits** | **Maximum Bounded Re-entrancy:** A System loops exactly up to the `MAX_RE_ENTRIES` limit and then halts. | Executive completes the frame successfully without throwing an exception. Validates the boundary condition. | `Edge_ReEntrancyExactlyAtLimit_Resolves` |
| **Exception Boundaries** | **Isolated System Crash:** System throws `std::exception`. | System marked as `FAILED`. Independent systems in the DAG continue execution normally. | `Failure_IsolatedNodeException_EngineSurvives` |
| **Exception Boundaries** | **Transitive Dependent Abortion:** System A crashes. System B reads A's output. System C reads B's output. | The Executive recursively aborts B and C. Both are marked `aborted_due_to_dependency` to prevent reading corrupted memory. | `Failure_DeepCascadingDependency_AbortsDownstream` |
| **Exception Boundaries** | **Multiple Independent Crashes:** System A and System B both crash. System C depends on neither. | The Executive logs both exceptions, fails both nodes, but successfully executes System C. | `Failure_MultipleIndependentCrashes` |
| **JIT Pre-Pass** | **Zero-Entity Blast Radius:** A System declares `IntentDelete` but its query matches 0 entities in the database. | The dynamic blast radius evaluates to `0`. No lock escalation occurs. The DAG evaluates purely on static bounds. | `Edge_JITDeleteNonExistent_NoEscalation` |
| **JIT Pre-Pass** | **Dynamic Component Overlap:** Entity spawned as Item, dynamically gains Physics. Deleted as Item. | JIT calculates blast radius to include Physics, dynamically injecting an edge to pause the Physics system. | `JITLockEscalation_DynamicIntersection` |
| **Graph Topology** | **Empty Task Graph:** Executive `execute_frame` called with 0 registered systems. | NOP. Frame executes in near-zero time without crashing. | `Edge_EmptyGraph_NoCrash` |
| **Access Violation** | **Undeclared Deletion / Insertion:** System calls `destroy_entity` without declaring `IntentDelete`. | `static_assert` fails compilation. The proxy mathematically isolates the database from rogue logic. | *Proven statically (cannot be GTest'd at runtime).* |
