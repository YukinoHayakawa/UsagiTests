# Usagi Engine: Virtualization via Layered Shadow Overlays

## 1\. The Immutable Foundation Constraint

Usagi Engine mandates that the ECS structures must be mathematically pure and trivially memory-mappable. When shipping a game (Layer 0), the resulting `base_game.mmap` is a read-only memory segment. To support game saves, patches, or dynamic state overrides, the engine must mutate the state matrix without violating the read-only constraint of the base file.

## 2\. The Identity vs. Storage Collapse

A naive implementation assumes patching an entity means creating a new entity with the same ID in a mutable patch file. This is physically impossible. The Usagi `EntityId` is a hardcoded 128-bit routing coordinate $(Domain, Partition, Page, Slot)$. It defines absolute physical memory offset. A mutable top-layer allocator cannot jump to a specific physical coordinate encoded by a bottom-layer domain.

## 3\. The Shadow Page Table (Virtual Entity Aggregation)

To resolve this, the Executive utilizes a `LayeredDatabaseAggregator`. Identity is decoupled from routing via transient RAM indices.

### 3.1 The Overlay Mask (Tombstones)

If the mutable top layer wishes to delete an entity from the read-only base layer ($E\_{base}$), it registers a Tombstone.

-   The Aggregator stores this in the `TransientShadowIndex` as a 64-bit `shadow_mask` mapped to $E\_{base}$'s Hive Page.
-   During $O(1)$ hardware iteration, the Base Layer evaluates its `active_lanes` and executes a bitwise intersection: `active_lanes &= ~shadow_mask`.
-   The deleted entity is mathematically stripped from the query stream in a single CPU cycle, completely preserving the immutability of the underlying base page.

### 3.2 The Redirect (Shadow Aliasing)

If the mutable layer wishes to structurally modify $E\_{base}$, it spans a brand new physical entity $E\_{patch}$ in its own append-only Hive Allocator.

-   The Aggregator registers an $O(1)$ Redirect: $E\_{base} \\to E\_{patch}$ in the `TransientShadowIndex`.
-   The Base Entity is implicitly tombstoned (hidden from iteration).
-   If a System holds an old Foreign Key pointing to $E\_{base}$, the Aggregator intercepts the `get_dynamic_mask(E_{base})` call, detects the redirect, and cleanly returns the data associated with $E\_{patch}$.

This virtualized abstraction guarantees that the 128-bit Bipartite Edges (Foreign Keys) remain completely valid across patches and save files, while restricting all physical disk mutations strictly to the top-most append-only database layer.
