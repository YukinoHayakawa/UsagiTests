# Usagi Engine: N-Layer Virtualization & Disjoint-Set Path Compression

## 1\. The Immutable Foundation Constraint

Traditional ECS architectures rely on heap mutation. When an entity's composition changes, it is moved to a new Archetype array, and its old memory is reclaimed.

This destroys binary serialization. If the base game data (`base.mmap`) is 10GB of geometry and static entities, modifying a single entity requires copying it into a delta-save file.

Usagi Engine treats the base layer ($L\_0$) as a mathematically immutable, memory-mapped hypergraph. Modifications (Patches, Saves, DLCs) are pushed as sequential, orthogonal memory domains ($L\_1 \\dots L\_N$). The active state of the universe is the topological union of all layers.

## 2\. The Virtual Page Table (Transient RAM Index)

To project the illusion of a single, flat ECS matrix to the Task Graph without physically copying memory, the `LayeredDatabaseAggregator` maintains an $O(1)$ Transient RAM Index. This acts exactly like a CPU's virtual page table, translating logical `EntityId` addresses to physical layer coordinates.

### 2.1 Tombstoning (The BMI1 Erasure)

When $L\_N$ destroys an entity $E$ that resides in $L\_{N-k}$, it cannot alter $L\_{N-k}$'s physical memory.

-   **Binary Serialization:** $L\_N$ spawns a physical entity possessing `CompShadowTombstone` and the target ID of $E$.
-   **RAM Index:** `shadowed_lanes` tracks a 64-bit mask for every physical memory page.
-   $O(1)$ **Execution:** When a System iterates over a page in $L\_{N-k}$, it executes `active_lanes &= ~shadowed_lanes[page_key]`. The CPU's hardware `TZCNT` (Count Trailing Zeros) instruction completely bypasses the dead entity without evaluating a single branch.

### 2.2 Redirects (The Physical Alias)

When an entity's structure mutates, $L\_N$ creates a "Patch Entity" with the new Component Mask and the `CompShadowRedirect` brand.

-   A Tombstone is instantly applied to the old coordinate.
-   A mapping is registered in the RAM index routing the old `EntityId` to the new `EntityId`.

## 3\. Disjoint-Set Path Compression (O(1) Alias Resolution)

A naive redirect implementation creates a linked list: $E\_{base} \\to E\_{dlc1} \\to E\_{savegame}$.

Querying the active mask of a deeply patched entity incurs $O(K)$ cache misses traversing the heap-allocated hash map. In pathological save-scumming scenarios, $K$ reaches thousands, triggering catastrophic frame drops.

Usagi Engine implements **Union-Find Path Compression** during the boot sequence (`rebuild_transient_indices`). The RAM index is flattened into three $O(1)$ bounds:

1.  **`terminal_alias_map[LogicalRoot] = ActiveTerminal`**: Resolves any base entity directly to its newest physical patch in $O(1)$ amortized time.
2.  **`logical_root_map[AnyAlias] = LogicalRoot`**: Reverse lookup. Ensures that if a system holds an intermediate patch pointer, it can instantly resolve the absolute base identity.
3.  **`alias_chains[LogicalRoot] = vector<EntityId>`**: A contiguous array of the entity's historical identities, pre-cached for Bipartite Edge aggregation.

## 4\. The Ghost Resurrection Anomaly (Necromantic Aliasing)

Because the engine utilizes path compression rather than strict generation counters ($\\gamma$) for identity resolution, it mathematically supports lock-free pointer resurrection.

**The Theorem:**

Let $E\_0$ be spawned in $L\_0$.

Let $L\_1$ execute `IntentDelete<E_0>`. $E\_0$ is tombstoned. `get_dynamic_mask(E_0)` yields `0`.

Let $L\_2$ execute `IntentAdd<E_0, CompPlayer>`.

-   The Aggregator fetches the existing mask (`0`).
-   It creates Patch $P\_2$ in $L\_2$ with mask `CompPlayer | CompShadowRedirect`.
-   It maps $E\_0 \\to P\_2$.

**The Proof:**

The tombstone on $E\_0$ remains physically intact, ensuring $L\_0$ iteration permanently skips the dead coordinate. However, $P\_2$ is fully active in $L\_2$. Any dangling `ForeignKey` edges held by other systems pointing to $E\_0$ are instantly, $O(1)$ routed to $P\_2$. The entity is resurrected, its structural state is wiped clean save for the new additions, and zero memory pointers are corrupted.

## 5\. Bipartite Edge Union (The Multi-Layer Scatter-Gather)

In an N-Layer architecture, Bipartite Relational Edges are fractured across domains.

If an AI agent $A\_{base}$ targets $B\_{base}$ in $L\_0$, and $A\_{base}$ is patched to $A\_{patch}$ in $L\_1$, the physical edge still resides in $L\_0$ keyed to $A\_{base}$.

When a system queries `get_outbound_edges(A_patch)`:

1.  The Aggregator reverse-resolves $A\_{patch} \\to A\_{base}$ using the `logical_root_map`.
2.  It fetches the pre-compiled `alias_chain`: $\[A\_{base}, A\_{patch}\]$.
3.  It executes a cross-product sweep over the chain and the layer stack. It queries $L\_0$ for edges attached to $A\_{base}$, and $L\_1$ for edges attached to $A\_{patch}$.
4.  It applies the $O(1)$ `is_tombstoned()` filter to mathematically drop any edges that were explicitly destroyed by higher layers.

This mathematically guarantees that Relational Graphs remain topologically unbroken across infinite Virtualization overlays, without ever physically duplicating base-layer relational arrays into the save file footprint.

## 6\. Querying Efficiency & Workload Dispatching

Evaluating the N-Layer database is a multi-dimensional intersection problem. It requires slicing the memory horizontally across `ComponentGroup` footprints, and vertically across the Virtualization stack.

### 6.1 Horizontal Slicing: The Column-Major Hive

Storing actual data requires strict Structure of Arrays (SoA) layout aligned to the 64-entity Hive Pages. The metadata and payload are defined as:

```
struct ComponentGroupMeta {
    uint64_t entities; // Active physical lanes
    uint64_t components[MAX_COMPONENTS]; // Sub-archetype component presence
    uint64_t aggregate_bloom_filter; // Bitwise OR of all active components
};

template<typename... Components>
struct HiveDataPage {
    std::tuple<std::array<Components, 64>...> data;
};
```

When querying an entity's inclusion, the engine evaluates `entities & components[query_index]`. Memory access is instantly resolved via the 128-bit coordinate: `page.data.get<T>()[slot]`.

### 6.2 Vertical Aggregation: The Bloom Filter Bypass

Iterating $L$ layers incurs a linear cache penalty. To dissolve this overhead, the `aggregate_bloom_filter` acts as an absolute exclusionary boundary.

If `(page.aggregate_bloom_filter & query_mask) != query_mask`, the CPU mathematically skips the entire 64-entity block without ever reading the component arrays or checking the `TransientShadowIndex`.

### 6.3 $O(1)$ Static Span Dispatch (Solving Thread Starvation)

Letting systems allocate `std::vector<EntityId>` inside their update loops destroys data-parallelism and halts the CPU with heap allocations.

The Usagi Task Graph natively severs memory layout from workload scheduling via **Span Dispatch**:

1.  **The Scheduler Calculation:** Before a System executes, the Executive knows the database has exactly 256 orthogonal partitions.
2.  **The Atomic Range:** The System node is pushed to the worker pool with an atomic counter initialized to 0.
3.  **Lock-Free Chunking:** Worker threads execute an atomic `fetch_add(N)` to claim a span of partitions `[start_partition, end_partition)`.
4.  **Direct Execution:** The worker iterates ONLY its assigned partitions, evaluates the `aggregate_bloom_filter`, and executes the System's lambda directly on the raw contiguous `HiveDataPage` arrays. Zero heap allocations. Perfect core saturation.
