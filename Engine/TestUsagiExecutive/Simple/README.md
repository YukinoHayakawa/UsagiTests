# Usagi Engine Executive: Mathematical Model & Empirical Proofs

## 1\. The Formal Mathematical Model

The Usagi Engine is modeled as a strictly discrete endomorphism $f: S \\to S$, where the state space $S$ is a sparse matrix mapping Entities to Components.
Let $E = \\{e\_0, e\_1, \\dots, e\_{N-1}\\}$ be the set of active entities.
Let $C = \\{c\_0, c\_1, \\dots, c\_{M-1}\\}$ be the set of component types.
The state matrix $S \\in \\mathbb{B}^{|E| \\times |C|}$ represents the structural footprint of the simulation, where $S\_{i,j} = 1$ if entity $e\_i$ possesses component $c\_j$.

### 1.1 The Task Graph (DAG) $G = (V, D)$

A single frame evaluates a set of System functions $V = \\{v\_0, v\_1, \\dots, v\_{K-1}\\}$.
Each system $v\_k$ explicitly declares its static memory bounds via C++26 reflection:
-   $R\_k \\subseteq C$ (Read Mask)
-   $W\_k \\subseteq C$ (Write Mask)
-   $\\Delta\_k \\subseteq C$ (Delete Intent Mask)
-   $\\Phi\_k$ (Orthogonal Data Access Policies, e.g., `Previous`, `Atomic`)

The topological compiler constructs the dependency edges $D \\subseteq V \\times V$.
For any two nodes $v\_i, v\_j$ where $i < j$ (registration order establishes the implicit baseline time-arrow), an edge $(v\_i, v\_j)$ exists iff:
$$(W\_i \\cap R\_j \\neq \\emptyset) \\lor (R\_i \\cap W\_j \\neq \\emptyset) \\lor (W\_i \\cap W\_j \\neq \\emptyset)$$
However, this strict intersection is mathematically relaxed by the orthogonal policies $\\Phi$:
-   **WAR/RAW Severing:** If $\\Phi\_j$ contains `Previous`, then $W\_i \\cap R\_j$ is severed (Read-After-Write dissolved).
-   **WAW Severing:** If $\\Phi\_i$ and $\\Phi\_j$ both contain `Atomic`, then $W\_i \\cap W\_j$ is severed.

### 1.2 Just-In-Time (JIT) Lock Escalation

To guarantee $O(1)$ immediate destruction without global synchronization points, the dynamic blast radius of a system is computed immediately before DAG generation.
Let $Q\_k = \\{e \\in E \\mid (S\_{e} \\cap R\_k = R\_k) \\land (S\_{e} \\cap \\Delta\_k \\neq \\emptyset)\\}$ be the set of entities matching the delete query of $v\_k$.
The JIT-expanded write mask $\\hat{W}\_k$ is defined as:
$$\\hat{W}\_k = W\_k \\cup \\bigcup\_{e \\in Q\_k} S\_e$$
This mathematically guarantees that if $v\_k$ destroys an entity possessing an undeclared component $c\_x$, the DAG automatically injects an edge to serialize $v\_k$ against any parallel system reading/writing $c\_x$.

### 1.3 Deterministic Spatial Partitioning

During execution, parallel threads generate new structural states. To preserve Amdahl's scaling and bit-exact determinism, entities are pushed to orthogonal staging partitions based on thread/workgroup index $p$:
$$P\_p = \\bigcup\_{m=1}^{s} \\text{spawn\\\_mask}\_m$$
At the end of the execution phase, the database mathematically flattens the partitions into the continuous set of active entities $E$:
$$E\_{t+1} = E\_t \\cup \\bigoplus\_{p=0}^{255} P\_p$$
Because the flattening loop strictly iterates from $p=0$ to $255$, the assigned `EntityId` values are monotonically identical across infinite runs, regardless of which physical hardware thread evaluated $P\_p$.

## 2\. Empirical Verification Boundaries

Our current C++ Google Test suite rigorously proves the following limits:
-   **Topological Integrity:** The `MemoryChunkGuard` fuzzing proves that WAW, RAW, and WAR edges are correctly identified and serialized. Not a single atomic data race slipped through 200 iterations of 256-thread randomized graph generation.
-   **JIT Completeness:** The `JitTrapdoor_DynamicInversion` test empirically proves that undeclared structural deletions force a runtime recalculation of $\\hat{W}\_k$, safely freezing parallel execution of intersecting domains.
-   **SEH Poison Diffusion:** Deep transitive cascading dependencies abort without inducing Stack Overflows (`Pathological_DeepLinearDependency_NoStackOverflow`) and without hanging the `std::condition_variable` thread barrier.
-   **Deterministic ID Assignment:** The `PartitionedAvalanche_DeterministicCommit` test proves that 2.56 million concurrently spawned entities map to exactly the same integer IDs in the exact same sequence.

## 3\. The Unproven Frontier (What the Tests Ignore)

The tests confirm the scheduler's *logical* safety. They completely ignore the realities of physical hardware exhaustion and cache entropy. The Executive is mathematically sound on paper, but it will crash or desync in production under the following untested conditions:

### 3.1 Entity ID Exhaustion (The $2^{32}$ Singularity)

The `EntityDatabase` uses `uint32_t next_id = 1;`. We have strictly proven that IDs are assigned deterministically. We have **not** proven what happens when `next_id` wraps around to `0`.
-   Does it crash?
-   If we implement an ID recycling pool, does the recycling pool maintain strict determinism across parallel requests?
-   If an ID is recycled, how do we prevent the **ABA Problem** where an old `ForeignKey` accidentally points to a newly recycled entity?
-   *Current Status:* Unhandled. Long-running simulations will eventually wrap and corrupt relational data.

### 3.2 Physical Memory Layout Entropy (The Associativity Trap)

We proved `EntityId` assignment is deterministic. But `EntityId` is just a logical integer. ECS performance relies on contiguous arrays of Component data.
When `db.commit_pending_spawns()` calls `create_entity_immediate()`, the physical `EntityRecord` is pushed to a `std::vector`.
-   If the underlying Component memory allocator uses thread-safe atomics to claim memory blocks out of a global arena, the *physical memory addresses* of the components will differ between runs based on OS thread scheduling.
-   If Systems iterate over components based on their physical memory layout (which ECS architectures do for cache efficiency), the iteration order will be non-deterministic.
-   Floating-point arithmetic is non-associative. If System A sums velocities `(A + B) + C` on run 1, and `A + (B + C)` on run 2 due to physical allocator interleaving, the network states will diverge.
-   *Current Status:* Untested. The current `EntityDatabase` mock uses `std::vector` for records, but lacks physical component array layout proofs.

### 3.3 False Sharing & L1 Cache Thrashing

The `MemoryChunkGuard` tracks `std::atomic<int> readers`. In a 64-core system, 64 threads simultaneously incrementing the exact same `readers` atomic variable will cause catastrophic L1 cache invalidation storms (False Sharing). The cache line containing that atomic will bounce across the CPU die, degrading the $O(1)$ lock-free promise into an $O(N)$ hardware stall.
-   *Current Status:* Untested. The topological proofs validate logic, not hardware caching efficiency.

### 3.4 Orthogonal Partition Saturation

We use `std::array<std::vector<ComponentMask>, 256> spawn_partitions`.
What happens if a rogue system queues $10^9$ particles into partition `0`? The `std::vector` will attempt to reallocate, potentially throwing `std::bad_alloc` inside the lock-free data-parallel phase.
If an `std::bad_alloc` occurs during the spawn queue phase, does the SEH firewall catch it without leaking the partially spawned entities?
-   *Current Status:* Untested. Out-Of-Memory (OOM) states inside the structural staging buffers are not evaluated.

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
