### Task Graph Executive: Failure Mode Matrix

| Failure Mode | Topological Cause | Executive Resolution Strategy | Status |
| :--- | :--- | :--- | :--- |
| **Infinite Re-entrancy** | A system queues structural mutations (e.g., spawns) but fails to consume the triggering condition, creating an unbounded Petri net. | The Executive tracks cycle depth. Upon breaching `MAX_RE_ENTRIES`, it aborts the frame and throws a fatal SEH/Exception. | Verified |
| **Isolated Node Exception** | A system's internal logic encounters an unhandled exception (e.g., Vulkan device loss, div-by-zero). | The Executive catches the exception, marks the node as `FAILED`, and allows independent parallel branches to continue execution. | Verified |
| **Cascading Dependent Abortion** | System A crashes. System B requires Read access to data written by System A. | If B runs, it reads corrupted state. The Executive mathematically infers the dependency edge and recursively aborts System B and all downstream nodes. | Verified |
| **Undeclared Mutation (Access Violation)** | A system attempts to `destroy` an entity but failed to declare `IntentDelete` in its `EntityQuery`. | Caught at compile-time via `DatabaseAccess` proxy using C++26 reflection (`static_assert`). Zero runtime cost. | Verified |
| **Overestimation Starvation** | JIT blast radius expands to cover the entire sparse matrix due to dense relational graphs. | Not a crash. Graph degrades to single-threaded sequential execution. Monitored via external `RealtimeProfilerService`. | Handled externally |
