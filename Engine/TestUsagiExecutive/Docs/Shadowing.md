# Usagi Engine: Virtualization via Layered Shadow Overlays

## 1\. The Immutable Foundation Constraint

Usagi Engine mandates that the ECS structures must be mathematically pure and trivially memory-mappable. When shipping a game (Layer 0), the resulting `base_game.mmap` is a read-only memory segment. To support game saves, patches, or dynamic state overrides, the engine must mutate the state matrix without violating the read-only constraint of the base file.

## 2\. The Shadow Page Table (Virtual Entity Aggregation)

To resolve this, the Executive utilizes a `LayeredDatabaseAggregator`. Identity is decoupled from routing via transient RAM indices.

-   **Tombstones:** A 64-bit `shadow_mask` applied via bitwise NOT during iteration (`active_lanes &= ~shadow_mask`). Erases read-only entities in $O(1)$ CPU cycles.
-   **Redirects:** A topological route mapping an immutable Base Entity to a mutable Patch Entity. Lookups for the old coordinate seamlessly fetch the new physical memory.

## 3\. Architectural Assumptions

1.  **Layer Topology:** Layers are strictly ordered $L\_0 \\to L\_N$. $L\_0$ is always the absolute base. Only $L\_N$ (the active top layer) is mutable.
2.  **Domain Mapping:** The `domain_node` bits inside `EntityId` rigorously correspond to the Layer Index.
3.  **No Cyclic Patches:** A redirect chain $E\_{base} \\to E\_{patch1} \\to E\_{patch2}$ forms a strict directed acyclic graph resolving to $L\_N$.

## 4\. Mathematical Capabilities

-   **Zero-Copy Base Data:** Unmodified base entities are never copied into the patch layer. RAM footprint scales exclusively with the delta of player actions.
-   $O(1)$ **Iteration Culling:** Iterating a shadow-masked sparse matrix incurs literally zero pipeline stalls, as the hardware `TZCNT` instruction natively skips masked bits.
-   **Non-Destructive Rollback:** Dropping $L\_1$ from RAM instantly restores the exact binary state of $L\_0$. Save scumming is mathematically instantaneous.
-   **Cross-Layer Component Extension:** $L\_N$ can append completely new `ComponentGroup` topologies to a base entity that never existed when $L\_0$ was compiled.

## 5\. Topological Limitations & Flaws (The Attack Vectors)

If you are writing the next test suite, attack these exact limitations. They are the current boundaries of the math.

### 5.1 The Shadow-Edge Collapse (Critical Flaw)

-   *The Math:* Let $E\_{base}$ own a Bipartite Edge $R\_{base}$ pointing to a Target. You mutate $E\_{base}$ via the Aggregator, generating $E\_{patch}$. The Aggregator registers the redirect $E\_{base} \\to E\_{patch}$.
-   *The Failure:* A system calls `get_outbound_edges(E_{base})`. The Aggregator resolves $E\_{base}$ to $E\_{patch}$, and queries $L\_N$ for the edges of $E\_{patch}$. It returns **Empty**.
-   *Why:* The original edge $R\_{base}$ was serialized in $L\_0$ with the source `EntityId` of $E\_{base}$. When the entity mutated, its identity split, but the base layer's adjacency list was not duplicated. The redirect severed the relational topology.
-   *The Fix Required:* `LayeredDatabaseAggregator::get_outbound_edges` must be rewritten to aggregate edges from *all* layers across the entire redirect chain, applying tombstone masks to explicitly deleted edges.

### 5.2 The Append-Only Memory Leak

-   *The Math:* Hive Allocators are append-only to guarantee lock-free $O(1)$ generation across 256 parallel partitions.
-   *The Failure:* If a high-frequency system (e.g., player health regeneration) updates an $L\_0$ entity 60 times a second, it generates 60 sequential $E\_{patch}$ entities in $L\_N$, leaving 59 dead holes in the sparse matrix.
-   *The Consequence:* The save file inflates infinitely. An offline compaction phase (Virtual Memory Defragmentation) must eventually be implemented to flatten the redirect chains and reclaim $L\_N$ pages.

### 5.3 Spatial Iteration Divergence

-   *The Math:* Base entities reside in low partitions. Patch entities reside in high partitions.
-   *The Failure:* If a system relies on the strict $O(1)$ spatial iteration order of $L\_0$ (e.g., rendering opaque objects front-to-back based on spawn order), patching an entity moves it to the end of the query result array, breaking the deterministic visual sort.
