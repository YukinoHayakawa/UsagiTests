# Usagi Engine: Topological & Virtualization Proof Matrix

This document defines the absolute test vectors required to mathematically prove the integrity of the Usagi Engine Executive, Layered Database Aggregator, and Bipartite HSM. We assume maximum hostility from the thread scheduler and the virtual memory bounds.

## Phase 1: The Bare Metal (Storage & Identity)

**Objective:** Prove that the pointer-free `VirtualMemoryArena` and the BMI1 `active_lanes` masks maintain a perfect 128-bit bijection without corrupting adjacent memory chunks.

-   **Test 1.1: The 64-Slot Singularity Boundary**

    -   *Action:* Spawn exactly 65 entities in a single partition (Partition 0).
    -   *Assertion:* Entity 64 resides at `(Page 0, Slot 63)`. Entity 65 resides at `(Page 1, Slot 0)`. The `active_lanes` bitmask for Page 0 must equal `0xFFFFFFFFFFFFFFFF`.

-   **Test 1.2: BMI1 Jump-Iteration Integrity**

    -   *Action:* Spawn 64 entities. Destroy all entities *except* Slot 0 and Slot 63.
    -   *Assertion:* A query over the database must execute exactly two loop iterations via `std::countr_zero`. It must mathematically skip the 62 dead lanes in $O(1)$ cycles.

-   **Test 1.3: Static POD Asserts**

    -   *Action:* Compile-time `consteval` check.
    -   *Assertion:* `std::is_trivially_copyable_v<HiveMetadataPage>` and `std::is_standard_layout_v<HiveMetadataPage>` must remain absolutely true to guarantee `mmap` ABI stability.

## Phase 2: Bipartite Graph Reduction & Transitive Closure

**Objective:** Prove that Reified Edge Entities prevent use-after-free corruption and that the JIT compiler successfully projects blast radii across hierarchical trees.

-   **Test 2.1: The** $O(1)$ **Relational Severance**

    -   *Action:* Entity A owns Edge E, which points to Entity B. Destroy Entity A.
    -   *Assertion:* Edge E is structurally annihilated from the database. Entity B remains untouched. The `TransientEdgeIndex` RAM adjacency list drops the A -> E routing entirely.

-   **Test 2.2: Transitive Closure JIT Projection**

    -   *Action:* Define a deep tree: A -> Edge1 -> B -> Edge2 -> C. A system declares `IntentDelete<A>`.
    -   *Assertion:* *Before* execution, the DAG compiler's `compute_transitive_closure` must traverse the RAM index and bitwise OR the masks of A, B, C, Edge1, and Edge2 into the system's `jit_write_mask`. If a parallel thread tries to read C, the DAG must mathematically block it.

## Phase 3: Virtualization (The Shadow Overlay)

**Objective:** Prove that a read-only Base Game layer ($L\_0$) can be non-destructively mutated by a Patch Layer ($L\_1$) via Tombstones and Redirects.

-   **Test 3.1: The Tombstone Masking Axiom**

    -   *Action:* Spawn Entity X in $L\_0$. Mount $L\_1$ as mutable. System deletes Entity X.
    -   *Assertion:* Entity X's physical memory in $L\_0$ must remain untouched. $L\_1$ must serialize a `CompShadowTombstone`. A global query must return 0 entities, because the `TransientShadowIndex` applies a bitwise NOT mask (`~shadow_mask`) over $L\_0$'s iteration lanes.

-   **Test 3.2: The Redirect Routing Axiom**

    -   *Action:* Entity X in $L\_0$ has `Health=100`. System adds `Poisoned` component to Entity X.
    -   *Assertion:* $L\_0$ remains untouched. $L\_1$ spawns Entity Y (The Patch) with `Health=100 | Poisoned` + `CompShadowRedirect`. If a foreign key queries `get_dynamic_mask(X)`, the Aggregator must instantly intercept and return the mask of Y.

## Phase 4: Binary Disk Serialization (The Amnesia Test)

**Objective:** Prove that the topological state survives a total loss of RAM.

-   **Test 4.1: The Cold Boot Reconstruction**

    -   *Action:* Execute Phase 2 and Phase 3 (creating Edges, Tombstones, and Redirects).
    -   *Action:* Dump $L\_1$'s `VirtualMemoryArena` to a raw `std::vector<uint8_t>`.
    -   *Action:* Nuke the engine. Call `clear_database()`, wipe the `TransientEdgeIndex`, wipe the `TransientShadowIndex`.
    -   *Action:* Import the raw bytes back into $L\_1$. Call `rebuild_transient_indices()`.
    -   *Assertion:* The RAM indices must perfectly reconstruct the Graph and the Page Table Overlays from the pure POD structs. Queries must yield the exact same masked and patched state as before the shutdown.

## Phase 5: Continuous Pipelining ($G\_{\\infty}$)

**Objective:** Prove the sliding-window multi-buffering severs artificial thread blocks.

-   **Test 5.1: Write-After-Read (WAR) Severance**

    -   *Action:* System A writes `CompVelocity`. System B reads `CompVelocity` with `DataAccessFlags::Previous`.
    -   *Assertion:* System B must NOT block System A in Frame N. System B reads the data committed in Frame N-1. The topological compiler must NOT generate a dependent edge between them.

-   **Test 5.2: Write-After-Write (WAW) Accumulation**

    -   *Action:* System A writes `CompDamage`. System B writes `CompDamage` with `DataAccessFlags::Atomic`.
    -   *Assertion:* The topological compiler severs the dependency edge. Both systems execute in parallel on the `TaskWorkerPool`, relying on hardware atomics to prevent torn writes.

## Phase 6: Hierarchical State Machine (HSM) Unrolling

**Objective:** Prove the declarative DSL expands into minimal, isolated JIT locks.

-   **Test 6.1: The Gecko Lifecycle Parallelism**

    -   *Action:* Register `TransFlee` and `TransDie` using `usagi::hsm::register_hsm`.
    -   *Assertion:* The DAG must produce two independent `SystemNode` instances. `TransFlee` must hold a write lock strictly for `StatePassive | StateFleeing`. `TransDie` must hold a write lock strictly for `StateFleeing | StateDead | CompGecko`.
    -   *Assertion:* The evaluation lambdas must correctly append the new state and strip the old state lock-free.
