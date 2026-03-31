#**Usagi Engine Executive: Mathematical Model & Empirical Proofs**

##**1****\\.** **The Formal Mathematical Model**

The Usagi Engine is modeled as a strictly discrete endomorphism $f: S \\\\to S$, where the state space $S$ is a sparse matrix mapping Entities to Components.

Let $E = \\\\{e\\\_0, e\\\_1, \\\\dots, e\\**\_**{N\-1}\\\\}$ be the set of active entities.

Let $C = \\\\{c\\\_0, c\\\_1, \\\\dots, c\\**\_**{M\-1}\\\\}$ be the set of component types.

The state matrix $S \\\\in \\\\**mathbb**{B}^{|E| \\\\times |C|}$ represents the structural footprint of the simulation, where $S\\**\_**{i,j} = 1$ if entity $e\\\_i$ possesses component $c\\\_j$.

###**1.1 The Task Graph (DAG) $G = (V, D)$**

A single frame evaluates a set of System functions $V = \\\\{v\\\_0, v\\\_1, \\\\dots, v\\**\_**{K\-1}\\\\}$.

Each system $v\\\_k$ explicitly declares its static memory bounds via C++26 reflection:

\-   $R\\\_k \\\\subseteq C$ (Read Mask)
\-   $W\\\_k \\\\subseteq C$ (Write Mask)
\-   $\\\\Delta\\\_k \\\\subseteq C$ (Delete Intent Mask)
\-   $\\\\Phi\\\_k$ (Orthogonal Data Access Policies, e.g., \`Previous\`, \`Atomic\`)

The topological compiler constructs the dependency edges $D \\\\subseteq V \\\\times V$.

For any two nodes $v\\\_i, v\\\_j$ where $i < j$ (registration order establishes the implicit baseline time-arrow), an edge $(v\\\_i, v\\\_j)$ exists iff:

$$(W\\**\_**i \\\\cap R\\**\_**j \\\\neq \\\\emptyset) \\\\lor (R\\**\_**i \\\\cap W\\**\_**j \\\\neq \\\\emptyset) \\\\lor (W\\**\_**i \\\\cap W\\**\_**j \\\\neq \\\\emptyset)$$

However, this strict intersection is mathematically relaxed by the orthogonal policies $\\\\Phi$:

\-   **\*\*****WAR/RAW Severing:****\*\*** If $\\\\Phi\\\_j$ contains \`Previous\`, then $W\\\_i \\\\cap R\\\_j$ is severed (Read-After-Write dissolved).
\-   **\*\*****WAW Severing:****\*\*** If $\\\\Phi\\\_i$ and $\\\\Phi\\\_j$ both contain \`Atomic\`, then $W\\\_i \\\\cap W\\\_j$ is severed.

###**1.2 Just-In-Time (JIT) Lock Escalation**

To guarantee $O(1)$ immediate destruction without global synchronization points, the dynamic blast radius of a system is computed immediately before DAG generation.

Let $Q\\\_k = \\\\{e \\\\in E \\\\mid(S\\**\_**{e} \\\\cap R\\\_k = R\\\_k) \\\\land(S\\**\_**{e} \\\\cap \\\\Delta\\\_k \\\\neq \\\\emptyset)\\\\}$ be the set of entities matching the delete query of $v\\\_k$.

The JIT-expanded write mask $\\\\**hat**{W}\\\_k$ is defined as:

$$\\\\hat{W}\\**\_**k = W\\**\_**k \\\\cup \\\\bigcup\\**\_**{e \\\\in Q\\**\_**k} S\\**\_**e$$

This mathematically guarantees that if $v\\\_k$ destroys an entity possessing an undeclared component $c\\\_x$, the DAG automatically injects an edge to serialize $v\\\_k$ against any parallel system reading/writing $c\\\_x$.

###**1.3 Deterministic Spatial Partitioning**

During execution, parallel threads generate new structural states. To preserve Amdahl's scaling and bit-exact determinism, entities are pushed to orthogonal staging partitions based on thread/workgroup index $p$:

$$P\\**\_**p = \\\\bigcup\\**\_**{m=1}^{s} \\\\text{spawn\\\\\\**\_**mask}\\**\_**m$$

At the end of the execution phase, the database mathematically flattens the partitions into the continuous set of active entities $E$:

$$E\\**\_**{t+1} = E\\**\_**t \\\\cup \\\\bigoplus\\**\_**{p=0}^{255} P\\**\_**p$$

Because the flattening loop strictly iterates from $p=0$ to $255$, the assigned \`EntityId\` values are monotonically identical across infinite runs, regardless of which physical hardware thread evaluated $P\\\_p$.

##**2****\\.** **Empirical Verification Boundaries**

Our current C++ Google Test suite rigorously proves the following limits:

\-   **\*\*****Topological Integrity:****\*\*** The \`MemoryChunkGuard\` fuzzing proves that WAW, RAW, and WAR edges are correctly identified and serialized. Not a single atomic data race slipped through 200 iterations of 256-thread randomized graph generation.
\-   **\*\*****JIT Completeness:****\*\*** The \`JitTrapdoor\_DynamicInversion\` test empirically proves that undeclared structural deletions force a runtime recalculation of $\\\\**hat**{W}\\\_k$, safely freezing parallel execution of intersecting domains.
\-   **\*\*****SEH Poison Diffusion:****\*\*** Deep transitive cascading dependencies abort without inducing Stack Overflows (\`Pathological\_DeepLinearDependency\_NoStackOverflow\`) and without hanging the \`std::condition\_variable\` thread barrier.
\-   **\*\*****Deterministic ID Assignment:****\*\*** The \`PartitionedAvalanche\_DeterministicCommit\` test proves that 2.56 million concurrently spawned entities map to exactly the same integer IDs in the exact same sequence.

##**3****\\.** **The Unproven Frontier (What the Tests Ignore)**

The tests confirm the scheduler's *\***logical**\** safety. They completely ignore the realities of physical hardware exhaustion and cache entropy. The Executive is mathematically sound on paper, but it will crash or desync in production under the following untested conditions:

###**3.1 Entity ID Exhaustion (The $2^{32}$ Singularity)**

The \`EntityDatabase\` uses \`uint32\_t next\_id = 1;\`. We have strictly proven that IDs are assigned deterministically. We have **\*\*****not****\*\*** proven what happens when \`next\_id\` wraps around to \`0\`.

\-   Does it crash?
\-   If we implement an ID recycling pool, does the recycling pool maintain strict determinism across parallel requests?
\-   If an ID is recycled, how do we prevent the **\*\*****ABA Problem****\*\*** where an old \`ForeignKey\` accidentally points to a newly recycled entity?

\-   *\***Current Status:**\** Unhandled. Long-running simulations will eventually wrap and corrupt relational data.

###**3.2 Physical Memory Layout Entropy (The Associativity Trap)**

We proved \`EntityId\` assignment is deterministic. But \`EntityId\` is just a logical integer. ECS performance relies on contiguous arrays of Component data.

When \`db.commit\_pending\_spawns()\` calls \`create\_entity\_immediate()\`, the physical \`EntityRecord\` is pushed to a \`std::vector\`.

\-   If the underlying Component memory allocator uses thread-safe atomics to claim memory blocks out of a global arena, the *\***physical memory addresses**\** of the components will differ between runs based on OS thread scheduling.
\-   If Systems iterate over components based on their physical memory layout (which ECS architectures do for cache efficiency), the iteration order will be non-deterministic.
\-   Floating-point arithmetic is non-associative. If System A sums velocities \`(A + B) + C\` on run 1, and \`A + (B + C)\` on run 2 due to physical allocator interleaving, the network states will diverge.

\-   *\***Current Status:**\** Untested. The current \`EntityDatabase\` mock uses \`std::vector\` for records, but lacks physical component array layout proofs.

###**3.3 False Sharing & L1 Cache Thrashing**

The \`MemoryChunkGuard\` tracks \`std::atomic<int> readers\`. In a 64-core system, 64 threads simultaneously incrementing the exact same \`readers\` atomic variable will cause catastrophic L1 cache invalidation storms (False Sharing). The cache line containing that atomic will bounce across the CPU die, degrading the $O(1)$ lock-free promise into an $O(N)$ hardware stall.

\-   *\***Current Status:**\** Untested. The topological proofs validate logic, not hardware caching efficiency.

###**3.4 Orthogonal Partition Saturation**

We use \`std::array<std::vector<ComponentMask>, 256> spawn\_partitions\`.

What happens if a rogue system queues $10^9$ particles into partition \`0\`? The \`std::vector\` will attempt to reallocate, potentially throwing \`std::bad\_alloc\` inside the lock-free data-parallel phase.

If an \`std::bad\_alloc\` occurs during the spawn queue phase, does the SEH firewall catch it without leaking the partially spawned entities?

\-   *\***Current Status:**\** Untested. Out-Of-Memory (OOM) states inside the structural staging buffers are not evaluated.
