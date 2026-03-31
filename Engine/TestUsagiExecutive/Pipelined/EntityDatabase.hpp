/*
 * Usagi Engine: Entity Database & Virtualization Core (C++26)
 * -----------------------------------------------------------------------------
 * Identity: 128-bit Distributed Orthogonal Metadata Directory (Pure Spatial).
 * Relations: Bipartite Graph Reduction via Reified Edge Entities.
 * Storage: Strictly Pointer-Free, Mmap-Ready ECS Structures (std::atomic_ref).
 * Virtualization: Layered Database Aggregation with Transient Shadow Overlays.
 * * Mathematical Guarantee: The memory chunks generated here are completely
 * trivially-copyable. They contain zero virtual pointers or std::atomic locks
 * that would corrupt upon binary disk restoration.
 */

#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <meta> // C++26 P2996R13
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace usagi
{
// -----------------------------------------------------------------------------
// Core Mathematical Primitives & Semantics
// -----------------------------------------------------------------------------

/* Shio: The 128-bit Pure Spatial Distributed Coordinate.
   Relies on strict Bipartite Edge Reification for relational integrity. */
struct alignas(16) EntityId
{
    uint32_t domain_node { 0 };
    uint32_t partition { 0 };
    uint32_t page { 0 };
    uint16_t slot { 0 };
    uint16_t hardware_flags { 0 };

    constexpr bool operator==(const EntityId &o) const
    {
        return domain_node == o.domain_node &&
            partition == o.partition &&
            page == o.page &&
            slot == o.slot;
    }

    constexpr bool operator!=(const EntityId &o) const { return !(*this == o); }
};

constexpr EntityId INVALID_ENTITY = {
    0xFFFF'FFFF, 0xFFFF'FFFF, 0xFFFF'FFFF, 0xFFFF, 0xFFFF
};

using ComponentMask = uint64_t;
using NodeIndex     = int64_t;

constexpr int      MAX_RE_ENTRIES       = 5;
constexpr uint32_t MAX_SPAWN_PARTITIONS = 256;
constexpr uint32_t HIVE_PAGE_SIZE       = 64;

enum class DataAccessFlags : uint16_t
{
    None       = 0,
    Read       = 1 << 0,
    Write      = 1 << 1,
    ReadWrite  = Read | Write,
    Host       = 1 << 2,
    Device     = 1 << 3,
    Discard    = 1 << 4,
    Previous   = 1 << 5, // Severs WAR/RAW for pipelining G_inf
    Atomic     = 1 << 6, // Severs WAW
    Accumulate = 1 << 7,
};

constexpr DataAccessFlags operator|(DataAccessFlags lhs, DataAccessFlags rhs)
{
    return static_cast<DataAccessFlags>(
        static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
}

constexpr DataAccessFlags operator&(DataAccessFlags lhs, DataAccessFlags rhs)
{
    return static_cast<DataAccessFlags>(
        static_cast<uint16_t>(lhs) & static_cast<uint16_t>(rhs));
}

template <typename T>
consteval ComponentMask get_component_bit();

// -----------------------------------------------------------------------------
// Declarative Query DSL (C++26)
// -----------------------------------------------------------------------------
template <typename... Ts>
consteval ComponentMask build_mask()
{
    return (0ull | ... | get_component_bit<Ts>());
}

template <typename... Ts>
struct Read
{
    static constexpr ComponentMask mask = build_mask<Ts...>();
    static constexpr bool is_read = true, is_write = false, is_delete = false,
                          is_policy = false;
};

template <typename... Ts>
struct Write
{
    static constexpr ComponentMask mask = build_mask<Ts...>();
    static constexpr bool is_read = false, is_write = true, is_delete = false,
                          is_policy = false;
};

template <typename... Ts>
struct IntentDelete
{
    static constexpr ComponentMask mask = build_mask<Ts...>();
    static constexpr bool is_read = false, is_write = false, is_delete = true,
                          is_policy = false;
};

template <typename... Ts>
struct IntentAdd
{
    static constexpr ComponentMask mask = build_mask<Ts...>();
    static constexpr bool is_read = false, is_write = true, is_add = true,
                          is_policy = false;
};

template <typename... Ts>
struct IntentRemove
{
    static constexpr ComponentMask mask = build_mask<Ts...>();
    static constexpr bool is_read = false, is_write = true, is_remove = true,
                          is_policy = false;
};

template <DataAccessFlags Flags>
struct AccessPolicy
{
    static constexpr DataAccessFlags flags = Flags;
    static constexpr bool is_read = false, is_write = false, is_policy = true;
};

template <typename... Args>
struct EntityQuery
{
    static constexpr std::array<std::meta::info, sizeof...(Args)> infos = {
        ^^Args...
    };
};

namespace meta_utils
{
template <typename Query>
consteval ComponentMask extract_read_mask()
{
    ComponentMask m = 0;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(Arg::is_read) m |= Arg::mask;
    }
    return m;
}

template <typename Query>
consteval ComponentMask extract_write_mask()
{
    ComponentMask m = 0;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(Arg::is_write) m |= Arg::mask;
    }
    return m;
}

template <typename Query>
consteval ComponentMask extract_delete_mask()
{
    ComponentMask m = 0;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(requires { Arg::is_delete; })
        {
            if constexpr(Arg::is_delete) m |= Arg::mask;
        }
    }
    return m;
}

template <typename Query>
consteval ComponentMask extract_add_mask()
{
    ComponentMask m = 0;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(requires { Arg::is_add; })
        {
            if constexpr(Arg::is_add) m |= Arg::mask;
        }
    }
    return m;
}

template <typename Query>
consteval ComponentMask extract_remove_mask()
{
    ComponentMask m = 0;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(requires { Arg::is_remove; })
        {
            if constexpr(Arg::is_remove) m |= Arg::mask;
        }
    }
    return m;
}

template <typename Query>
consteval DataAccessFlags extract_flags()
{
    DataAccessFlags f = DataAccessFlags::None;
    template for(constexpr auto arg_meta : Query::infos)
    {
        using Arg = [:arg_meta:];
        if constexpr(Arg::is_policy) f = f | Arg::flags;
    }
    return f;
}
} // namespace meta_utils

// -----------------------------------------------------------------------------
// Internal Virtualization Components (For Serialization)
// -----------------------------------------------------------------------------
struct CompShadowTombstone
{
    EntityId target;
};

template <>
consteval ComponentMask get_component_bit<CompShadowTombstone>()
{
    return 1ull << 62;
}

struct CompShadowRedirect
{
    EntityId target;
};

template <>
consteval ComponentMask get_component_bit<CompShadowRedirect>()
{
    return 1ull << 63;
}
} // namespace usagi

// Hash specialization
namespace std
{
template <>
struct hash<usagi::EntityId>
{
    size_t operator()(const usagi::EntityId &id) const
    {
        uint64_t w1 =
            (static_cast<uint64_t>(id.domain_node) << 32) | id.partition;
        uint64_t w2 = (static_cast<uint64_t>(id.page) << 32) |
            (static_cast<uint64_t>(id.slot) << 16) |
            id.hardware_flags;
        return hash<uint64_t>()(w1) ^ (hash<uint64_t>()(w2) << 1);
    }
};
} // namespace std

namespace usagi
{
// -----------------------------------------------------------------------------
// Transient Runtime Indices (RAM Only)
// -----------------------------------------------------------------------------

class TransientEdgeIndex
{
    // The Bipartite Graph Adjacency List
    std::unordered_multimap<EntityId, EntityId> adjacency_list;

public:
    void register_edge(EntityId source, EntityId edge)
    {
        adjacency_list.insert({ source, edge });
    }

    void remove_all_edges_for_source(EntityId source)
    {
        adjacency_list.erase(source);
    }

    std::vector<EntityId> get_outbound_edges(EntityId source) const
    {
        std::vector<EntityId> edges;
        auto                  range = adjacency_list.equal_range(source);
        for(auto it = range.first; it != range.second; ++it)
            edges.push_back(it->second);
        return edges;
    }

    void clear() { adjacency_list.clear(); }
};

/* Shio: O(1) Disjoint-Set Path Compression applied to the Virtual Page Table.
   This mathematically eradicates N-Layer linked-list traversal overhead. */
class TransientShadowIndex
{
    // The Virtual Page Table Overlay
    std::unordered_map<uint64_t, uint64_t> shadowed_lanes;

    // O(1) Flattened Caches
    std::unordered_map<EntityId, EntityId>              terminal_alias_map;
    std::unordered_map<EntityId, EntityId>              logical_root_map;
    std::unordered_map<EntityId, std::vector<EntityId>> alias_chains;

    constexpr uint64_t build_page_key(const EntityId &id) const
    {
        return (static_cast<uint64_t>(id.domain_node) << 32) |
            (static_cast<uint64_t>(id.partition) << 16) |
            id.page;
    }

public:
    void add_tombstone(EntityId base_entity)
    {
        uint64_t key = build_page_key(base_entity);
        shadowed_lanes[key] |= (1ull << base_entity.slot);
    }

    void add_redirect(EntityId base_entity, EntityId patch_entity)
    {
        add_tombstone(base_entity);

        // 1. Resolve absolute logical root in O(1) via the cache.
        EntityId logical_root = get_logical_root(base_entity);

        // 2. Overwrite the O(1) terminal alias pointer, compressing the path.
        terminal_alias_map[logical_root] = patch_entity;

        // 3. Map the newly spawned patch directly back to the absolute root.
        logical_root_map[patch_entity] = logical_root;

        // 4. Pre-cache the contiguous identity chain for instant Bipartite Edge
        // aggregation.
        auto &chain = alias_chains[logical_root];
        if(chain.empty()) chain.push_back(logical_root);
        chain.push_back(patch_entity);
    }

    uint64_t get_shadow_mask(
        uint32_t domain, uint32_t partition, uint32_t page) const
    {
        uint64_t key = (static_cast<uint64_t>(domain) << 32) |
            (static_cast<uint64_t>(partition) << 16) |
            page;
        auto it = shadowed_lanes.find(key);
        return it != shadowed_lanes.end() ? it->second : 0ull;
    }

    // Shio: Explicit $O(1)$ verification if an entity (e.g. an Edge) was killed
    // by an upper layer
    bool is_tombstoned(EntityId id) const
    {
        uint64_t key = build_page_key(id);
        auto     it  = shadowed_lanes.find(key);
        if(it != shadowed_lanes.end())
        {
            return (it->second & (1ull << id.slot)) != 0;
        }
        return false;
    }

    // Yukino: Strict O(1) lookup. No recursive or while loop needed.
    EntityId get_logical_root(EntityId any_alias) const
    {
        auto it = logical_root_map.find(any_alias);
        return it != logical_root_map.end() ? it->second : any_alias;
    }

    std::optional<EntityId> resolve_redirect(EntityId any_alias) const
    {
        EntityId root = get_logical_root(any_alias);
        auto     it   = terminal_alias_map.find(root);
        if(it != terminal_alias_map.end()) return it->second;
        return std::nullopt;
    }

    std::vector<EntityId> get_redirect_chain(EntityId any_alias) const
    {
        EntityId root = get_logical_root(any_alias);
        auto     it   = alias_chains.find(root);
        if(it != alias_chains.end()) return it->second;
        return { root }; // Just the root itself if no chain exists
    }

    void clear()
    {
        shadowed_lanes.clear();
        terminal_alias_map.clear();
        logical_root_map.clear();
        alias_chains.clear();
    }
};

// -----------------------------------------------------------------------------
// Hive Allocator Metadata Structures (Strictly Pointer-Free / Memory-Mappable)
// -----------------------------------------------------------------------------

// Yukino: Pure POD. No std::atomic allowed here. This guarantees the struct
// can be mmap'd directly from disk without ABI corruption or locking UB.
struct HiveMetadataPage
{
    alignas(8) std::array<ComponentMask, HIVE_PAGE_SIZE> masks;
    alignas(8) uint64_t active_lanes;
};

consteval void assert_pod_structures()
{
    static_assert(
        std::is_trivially_copyable_v<HiveMetadataPage>,
        "HiveMetadataPage MUST be trivially copyable for mmap.");
    static_assert(
        std::is_standard_layout_v<HiveMetadataPage>,
        "HiveMetadataPage MUST be standard layout for mmap.");
}

// Simulates a POSIX mmap / Windows MapViewOfFile memory region
class VirtualMemoryArena
{
    // Yukino: Eradicated std::vector to mathematically bypass libc++
    // value-initialization loops. std::make_unique_for_overwrite provides raw
    // uninitialized blocks comparable to mmap.
    std::unique_ptr<uint8_t[]> memory;
    size_t                     capacity_pages { 0 };
    size_t                     allocated_pages { 0 };

public:
    VirtualMemoryArena(size_t initial_pages = 4)
    {
        capacity_pages = initial_pages;
        memory         = std::make_unique_for_overwrite<uint8_t[]>(
            capacity_pages * sizeof(HiveMetadataPage));
    }

    HiveMetadataPage *allocate_page()
    {
        if(allocated_pages >= capacity_pages)
        {
            size_t new_capacity = capacity_pages * 2;
            auto   new_memory   = std::make_unique_for_overwrite<uint8_t[]>(
                new_capacity * sizeof(HiveMetadataPage));

            if(allocated_pages > 0)
            {
                std::memcpy(
                    new_memory.get(),
                    memory.get(),
                    allocated_pages * sizeof(HiveMetadataPage));
            }
            memory         = std::move(new_memory);
            capacity_pages = new_capacity;
        }

        uint8_t *ptr =
            memory.get() + (allocated_pages * sizeof(HiveMetadataPage));
        allocated_pages++;
        // Explicitly start lifetime as pure data
        auto *page         = new (ptr) HiveMetadataPage();
        page->active_lanes = 0;
        for(auto &m : page->masks)
            m = 0;
        return page;
    }

    HiveMetadataPage *get_page(uint32_t page_idx) const
    {
        if(page_idx >= allocated_pages) return nullptr;
        // const_cast needed because the arena represents raw mutable hardware
        // RAM mapped by the OS
        return reinterpret_cast<HiveMetadataPage *>(
            memory.get() + (page_idx * sizeof(HiveMetadataPage)));
    }

    size_t page_count() const { return allocated_pages; }

    void clear() { allocated_pages = 0; }

    // Shio: Binary I/O simulation for Game Saves
    std::vector<uint8_t> export_memory() const
    {
        std::vector<uint8_t> dump(allocated_pages * sizeof(HiveMetadataPage));
        if(allocated_pages > 0)
        {
            std::memcpy(dump.data(), memory.get(), dump.size());
        }
        return dump;
    }

    void import_memory(const std::vector<uint8_t> &dump)
    {
        allocated_pages = dump.size() / sizeof(HiveMetadataPage);
        capacity_pages  = allocated_pages + 4;
        memory          = std::make_unique_for_overwrite<uint8_t[]>(
            capacity_pages * sizeof(HiveMetadataPage));
        if(allocated_pages > 0)
        {
            std::memcpy(memory.get(), dump.data(), dump.size());
        }
    }
};

struct EntityPartition
{
    VirtualMemoryArena arena;
    uint32_t           current_page_idx { 0 };
    uint8_t            current_slot_idx { 0 };

    EntityPartition() { arena.allocate_page(); }
};

// -----------------------------------------------------------------------------
// Component Storage Mock (For Serialization Proofs)
// -----------------------------------------------------------------------------
// In a real engine, this is another set of VirtualMemoryArenas indexed by
// ComponentType. We mock it here via unordered_map just to prove the
// topological lifecycle.
struct ComponentPayloadStorage
{
    std::unordered_map<EntityId, CompShadowTombstone> tombstones;
    std::unordered_map<EntityId, CompShadowRedirect>  redirects;

    // For Edges
    std::unordered_map<EntityId, EntityId> edge_sources;
    std::unordered_map<EntityId, EntityId> edge_targets;

    void clear()
    {
        tombstones.clear();
        redirects.clear();
        edge_sources.clear();
        edge_targets.clear();
    }
};

// -----------------------------------------------------------------------------
// Single Layer Entity Database
// -----------------------------------------------------------------------------
class EntityDatabase
{
    std::array<EntityPartition, MAX_SPAWN_PARTITIONS> partitions;
    uint32_t                                          local_domain_node { 0 };

    struct DeferredSpawn
    {
        ComponentMask mask;
        uint32_t      partition_id;
    };

    struct DeferredEdge
    {
        EntityId      source;
        EntityId      edge;
        EntityId      target;
        ComponentMask mask;
    };

    std::array<std::vector<DeferredSpawn>, MAX_SPAWN_PARTITIONS> spawn_queues;
    std::vector<DeferredEdge> edge_spawn_queue;
    bool                      has_spawns_flag {
        false
    }; // Pure bool, mathematically coerced to atomic at call sites

public:
    TransientEdgeIndex      transient_edges;
    ComponentPayloadStorage payloads;

    EntityDatabase()                                      = default;
    EntityDatabase(const EntityDatabase &)                = delete;
    EntityDatabase &operator=(const EntityDatabase &)     = delete;
    EntityDatabase(EntityDatabase &&) noexcept            = default;
    EntityDatabase &operator=(EntityDatabase &&) noexcept = default;

    void set_local_domain(uint32_t node_id) { local_domain_node = node_id; }

    uint32_t get_domain() const { return local_domain_node; }

    EntityId create_entity_immediate(
        ComponentMask initial_mask, uint32_t partition_id)
    {
        EntityPartition &part = partitions[partition_id];
        if(part.current_slot_idx >= HIVE_PAGE_SIZE)
        {
            part.arena.allocate_page();
            part.current_page_idx++;
            part.current_slot_idx = 0;
        }

        uint32_t          p_idx = part.current_page_idx;
        uint16_t          s_idx = part.current_slot_idx++;
        HiveMetadataPage *page  = part.arena.get_page(p_idx);

        // Mutate pure POD securely using atomic_ref
        std::atomic_ref<ComponentMask>(page->masks[s_idx])
            .store(initial_mask, std::memory_order_release);
        std::atomic_ref<uint64_t>(page->active_lanes)
            .fetch_or(1ull << s_idx, std::memory_order_release);

        return EntityId { local_domain_node, partition_id, p_idx, s_idx, 0 };
    }

    void destroy_entity_immediate(EntityId id)
    {
        if(id.domain_node != local_domain_node ||
            id.partition >= MAX_SPAWN_PARTITIONS)
            return;
        HiveMetadataPage *page =
            partitions[id.partition].arena.get_page(id.page);
        if(!page) return;

        std::atomic_ref<ComponentMask>(page->masks[id.slot])
            .store(0, std::memory_order_release);
        std::atomic_ref<uint64_t>(page->active_lanes)
            .fetch_and(~(1ull << id.slot), std::memory_order_release);
        transient_edges.remove_all_edges_for_source(id);
    }

    std::vector<EntityId> query_entities(
        ComponentMask               required_mask,
        const TransientShadowIndex *shadow_index = nullptr) const
    {
        std::vector<EntityId> results;
        for(uint32_t p = 0; p < MAX_SPAWN_PARTITIONS; ++p)
        {
            const EntityPartition &part = partitions[p];
            for(uint32_t page_idx = 0; page_idx < part.arena.page_count();
                ++page_idx)
            {
                HiveMetadataPage *page = part.arena.get_page(page_idx);
                uint64_t active = std::atomic_ref<uint64_t>(page->active_lanes)
                                      .load(std::memory_order_acquire);

                if(shadow_index)
                    active &= ~shadow_index->get_shadow_mask(
                        local_domain_node, p, page_idx);

                while(active)
                {
                    uint16_t slot =
                        static_cast<uint16_t>(std::countr_zero(active));
                    active &= active - 1;

                    ComponentMask current_mask =
                        std::atomic_ref<ComponentMask>(page->masks[slot])
                            .load(std::memory_order_acquire);
                    if(required_mask == 0 ||
                        (current_mask & required_mask) == required_mask)
                    {
                        results.push_back(
                            EntityId {
                                local_domain_node, p, page_idx, slot, 0 });
                    }
                }
            }
        }
        return results;
    }

    ComponentMask get_dynamic_mask_raw(EntityId id) const
    {
        if(id.domain_node != local_domain_node ||
            id.partition >= MAX_SPAWN_PARTITIONS)
            return 0;
        const HiveMetadataPage *page =
            partitions[id.partition].arena.get_page(id.page);
        if(!page) return 0;

        if((std::atomic_ref<uint64_t>(
                const_cast<uint64_t &>(page->active_lanes))
                   .load(std::memory_order_acquire) &
               (1ull << id.slot)) != 0)
        {
            return std::atomic_ref<ComponentMask>(
                const_cast<ComponentMask &>(page->masks[id.slot]))
                .load(std::memory_order_acquire);
        }
        return 0;
    }

    void add_component_immediate(EntityId id, ComponentMask comp_mask)
    {
        if(id.domain_node != local_domain_node ||
            id.partition >= MAX_SPAWN_PARTITIONS)
            return;
        HiveMetadataPage *page =
            partitions[id.partition].arena.get_page(id.page);
        std::atomic_ref<ComponentMask>(page->masks[id.slot])
            .fetch_or(comp_mask, std::memory_order_release);
    }

    void remove_component_immediate(EntityId id, ComponentMask comp_mask)
    {
        if(id.domain_node != local_domain_node ||
            id.partition >= MAX_SPAWN_PARTITIONS)
            return;
        HiveMetadataPage *page =
            partitions[id.partition].arena.get_page(id.page);
        std::atomic_ref<ComponentMask>(page->masks[id.slot])
            .fetch_and(~comp_mask, std::memory_order_release);
    }

    void queue_spawn(ComponentMask initial_mask, uint32_t partition_id)
    {
        spawn_queues[partition_id].push_back({ initial_mask, partition_id });
        std::atomic_ref<bool>(has_spawns_flag)
            .store(true, std::memory_order_relaxed);
    }

    void queue_edge_registration(
        EntityId source, EntityId target, ComponentMask edge_mask)
    {
        edge_spawn_queue.push_back(
            { source, INVALID_ENTITY, target, edge_mask });
        std::atomic_ref<bool>(has_spawns_flag)
            .store(true, std::memory_order_relaxed);
    }

    bool has_pending_spawns() const
    {
        return std::atomic_ref<bool>(const_cast<bool &>(has_spawns_flag))
            .load(std::memory_order_relaxed);
    }

    void commit_pending_spawns()
    {
        if(!has_pending_spawns()) return;
        for(uint32_t p = 0; p < MAX_SPAWN_PARTITIONS; ++p)
        {
            for(auto &spawn : spawn_queues[p])
                create_entity_immediate(spawn.mask, spawn.partition_id);
            spawn_queues[p].clear();
        }
        for(auto &edge : edge_spawn_queue)
        {
            EntityId edge_id = create_entity_immediate(edge.mask, 0);
            payloads.edge_sources[edge_id] = edge.source;
            payloads.edge_targets[edge_id] = edge.target;
            transient_edges.register_edge(edge.source, edge_id);
        }
        edge_spawn_queue.clear();
        std::atomic_ref<bool>(has_spawns_flag)
            .store(false, std::memory_order_relaxed);
    }

    void clear_database()
    {
        for(auto &part : partitions)
        {
            part.arena.clear();
            part.arena.allocate_page();
            part.current_page_idx = 0;
            part.current_slot_idx = 0;
        }
        for(auto &q : spawn_queues)
            q.clear();
        edge_spawn_queue.clear();
        transient_edges.clear();
        payloads.clear();
        std::atomic_ref<bool>(has_spawns_flag)
            .store(false, std::memory_order_relaxed);
    }

    // --- Serialization Mechanics ---
    std::vector<std::vector<uint8_t>> export_layer_memory() const
    {
        std::vector<std::vector<uint8_t>> dumps;
        for(const auto &part : partitions)
            dumps.push_back(part.arena.export_memory());
        return dumps;
    }

    void import_layer_memory(
        const std::vector<std::vector<uint8_t>> &dumps,
        const ComponentPayloadStorage           &imported_payloads)
    {
        clear_database();
        for(size_t i = 0; i < dumps.size() && i < MAX_SPAWN_PARTITIONS; ++i)
        {
            partitions[i].arena.import_memory(dumps[i]);
            partitions[i].current_page_idx =
                partitions[i].arena.page_count() > 0
                ? partitions[i].arena.page_count() - 1
                : 0;
            partitions[i].current_slot_idx = HIVE_PAGE_SIZE;
        }
        payloads = imported_payloads;
    }
};

// -----------------------------------------------------------------------------
// Layered Database Aggregator (Virtualization Controller)
// -----------------------------------------------------------------------------
class LayeredDatabaseAggregator
{
    std::vector<std::unique_ptr<EntityDatabase>> layers;
    TransientShadowIndex                         shadow_index;

    struct ShadowRedirectPayload
    {
        EntityId      base_id;
        ComponentMask new_mask;
        uint32_t      partition;
    };

    struct TombstonePayload
    {
        EntityId base_id;
    };

    std::vector<ShadowRedirectPayload> shadow_queue;
    std::vector<TombstonePayload>      tombstone_queue;
    bool has_shadow_mutations_flag { false }; // Eradicated std::atomic

public:
    LayeredDatabaseAggregator()
    {
        layers.push_back(std::make_unique<EntityDatabase>());
        layers[0]->set_local_domain(0);
    }

    void mount_readonly_layer(std::unique_ptr<EntityDatabase> db)
    {
        if(layers.back()->query_entities(0xFFFF'FFFF'FFFF'FFFF).size() > 0)
        {
            throw std::runtime_error(
                "FATAL: Cannot mount read-only layer underneath an active "
                "patch layer. "
                "Use push_new_mutable_patch_layer() dynamically, or configure "
                "mounts strictly during initialization.");
        }
        auto top = std::move(layers.back());
        layers.pop_back();
        layers.push_back(std::move(db));
        layers.push_back(std::move(top));
        for(uint32_t i = 0; i < layers.size(); ++i)
            layers[i]->set_local_domain(i);
    }

    /* Yukino: Safely appends a new mutable patch layer (e.g. D1 -> D2)
       without invoking deleted std::move semantics on active atomic matrices.
     */
    void push_new_mutable_patch_layer()
    {
        uint32_t new_domain = static_cast<uint32_t>(layers.size());
        auto     new_layer  = std::make_unique<EntityDatabase>();
        new_layer->set_local_domain(new_domain);
        layers.push_back(std::move(new_layer));
    }

    void rebuild_transient_indices()
    {
        shadow_index.clear();
        for(auto &layer : layers)
            layer->transient_edges.clear();

        // Shio: Sequential top-down scan naturally enforces O(1) path
        // compression. As we process newer layers, the add_redirect function
        // instantly resolves intermediate aliases to their logical root,
        // collapsing the history.
        for(auto &layer : layers)
        {
            for(const auto &[edge_id, source_id] : layer->payloads.edge_sources)
            {
                layer->transient_edges.register_edge(source_id, edge_id);
            }
            auto tombstones =
                layer->query_entities(build_mask<CompShadowTombstone>());
            for(EntityId t_id : tombstones)
            {
                EntityId target = layer->payloads.tombstones[t_id].target;
                shadow_index.add_tombstone(target);
            }
            auto redirects =
                layer->query_entities(build_mask<CompShadowRedirect>());
            for(EntityId r_id : redirects)
            {
                EntityId target = layer->payloads.redirects[r_id].target;
                shadow_index.add_redirect(target, r_id);
            }
        }
    }

    EntityDatabase &get_mutable_layer() { return *layers.back(); }

    const TransientShadowIndex &get_shadow_index() const
    {
        return shadow_index;
    }

    std::vector<EntityId> query_entities(ComponentMask required_mask) const
    {
        std::vector<EntityId> aggregated;
        for(const auto &layer : layers)
        {
            const TransientShadowIndex *idx_ptr =
                (layer->get_domain() == layers.back()->get_domain())
                ? nullptr
                : &shadow_index;
            auto sub_results = layer->query_entities(required_mask, idx_ptr);
            aggregated.insert(
                aggregated.end(), sub_results.begin(), sub_results.end());
        }
        return aggregated;
    }

    ComponentMask get_dynamic_mask(EntityId id) const
    {
        if(auto patch_id = shadow_index.resolve_redirect(id))
        {
            // Shio: O(1) array lookup based on the resolved alias's native
            // domain.
            if(patch_id->domain_node < layers.size())
            {
                return layers[patch_id->domain_node]->get_dynamic_mask_raw(
                    *patch_id);
            }
            return 0;
        }

        if(shadow_index.is_tombstoned(id)) return 0;

        // Shio: O(1) array lookup. Eradicated the O(M) iterative search.
        // Fallback to the original entity's native domain if unpatched
        if(id.domain_node < layers.size())
        {
            return layers[id.domain_node]->get_dynamic_mask_raw(id);
        }
        return 0;
    }

    /* Shio: The topological union fix for Shadow-Edge Collapse.
       We trace the entire N-Layer redirect chain, aggregate the edges from all
       historical aliases, and apply explicit Tombstone filtering to respect
       edge deletions. */
    std::vector<EntityId> get_outbound_edges(EntityId source) const
    {
        std::vector<EntityId> aggregated_edges;

        // Shio: Mathematically reduce the passed coordinate to the base logical
        // identity BEFORE projecting the forward alias chain. This seals the
        // identity leak.
        EntityId logical_root = shadow_index.get_logical_root(source);
        std::vector<EntityId> identity_chain =
            shadow_index.get_redirect_chain(logical_root);

        // Shio: Restored the complete topological cross-product.
        // An edge belonging to this logical entity could be physically stored
        // in ANY layer, and could be keyed under ANY of the aliases in the
        // chain.
        for(const auto &layer : layers)
        {
            for(EntityId alias : identity_chain)
            {
                auto layer_edges =
                    layer->transient_edges.get_outbound_edges(alias);
                for(EntityId edge_id : layer_edges)
                {
                    if(!shadow_index.is_tombstoned(edge_id))
                    {
                        aggregated_edges.push_back(edge_id);
                    }
                }
            }
        }
        return aggregated_edges;
    }

    // --- Write Interceptors (Enforcing Terminal Alias Resolution) ---

    void destroy_entity(EntityId id)
    {
        EntityId logical_root = shadow_index.get_logical_root(id);
        EntityId terminal_id  = logical_root;
        if(auto alias = shadow_index.resolve_redirect(logical_root))
            terminal_id = *alias;

        if(terminal_id.domain_node == layers.back()->get_domain())
            layers.back()->destroy_entity_immediate(terminal_id);
        else
        {
            tombstone_queue.push_back({ terminal_id });
            std::atomic_ref<bool>(has_shadow_mutations_flag)
                .store(true, std::memory_order_relaxed);
        }
    }

    void add_component(EntityId id, ComponentMask mask, uint32_t partition)
    {
        EntityId logical_root = shadow_index.get_logical_root(id);
        EntityId terminal_id  = logical_root;
        if(auto alias = shadow_index.resolve_redirect(logical_root))
            terminal_id = *alias;

        if(terminal_id.domain_node == layers.back()->get_domain())
            layers.back()->add_component_immediate(terminal_id, mask);
        else
        {
            ComponentMask existing = get_dynamic_mask(terminal_id);
            shadow_queue.push_back({ terminal_id, existing | mask, partition });
            std::atomic_ref<bool>(has_shadow_mutations_flag)
                .store(true, std::memory_order_relaxed);
        }
    }

    void remove_component(EntityId id, ComponentMask mask, uint32_t partition)
    {
        EntityId logical_root = shadow_index.get_logical_root(id);
        EntityId terminal_id  = logical_root;
        if(auto alias = shadow_index.resolve_redirect(logical_root))
            terminal_id = *alias;

        if(terminal_id.domain_node == layers.back()->get_domain())
            layers.back()->remove_component_immediate(terminal_id, mask);
        else
        {
            ComponentMask existing = get_dynamic_mask(terminal_id);
            shadow_queue.push_back(
                { terminal_id, existing & ~mask, partition });
            std::atomic_ref<bool>(has_shadow_mutations_flag)
                .store(true, std::memory_order_relaxed);
        }
    }

    bool has_pending_spawns() const
    {
        return layers.back()->has_pending_spawns() ||
            std::atomic_ref<bool>(const_cast<bool &>(has_shadow_mutations_flag))
                .load(std::memory_order_relaxed);
    }

    void commit_pending_spawns()
    {
        layers.back()->commit_pending_spawns();

        if(std::atomic_ref<bool>(has_shadow_mutations_flag)
                .load(std::memory_order_relaxed))
        {
            for(const auto &tb : tombstone_queue)
            {
                EntityId tb_id = layers.back()->create_entity_immediate(
                    build_mask<CompShadowTombstone>(), 0);
                layers.back()->payloads.tombstones[tb_id] = { tb.base_id };
                shadow_index.add_tombstone(tb.base_id);
            }
            for(const auto &patch : shadow_queue)
            {
                EntityId new_id = layers.back()->create_entity_immediate(
                    patch.new_mask | build_mask<CompShadowRedirect>(), 0);
                layers.back()->payloads.redirects[new_id] = { patch.base_id };
                shadow_index.add_redirect(patch.base_id, new_id);
            }
            tombstone_queue.clear();
            shadow_queue.clear();
            std::atomic_ref<bool>(has_shadow_mutations_flag)
                .store(false, std::memory_order_relaxed);
        }
    }
};

// -----------------------------------------------------------------------------
// Compile-Time Capability Proxy
// -----------------------------------------------------------------------------
template <typename SystemType>
class DatabaseAccess
{
    LayeredDatabaseAggregator &db_ref;
    uint32_t                   partition_idx;

public:
    explicit DatabaseAccess(
        LayeredDatabaseAggregator &db, uint32_t partition = 0)
        : db_ref(db), partition_idx(partition)
    {
    }

    std::vector<EntityId> query_entities(ComponentMask mask) const
    {
        return db_ref.query_entities(mask);
    }

    void queue_spawn(ComponentMask initial_mask)
    {
        db_ref.get_mutable_layer().queue_spawn(initial_mask, partition_idx);
    }

    void destroy_entity(EntityId id)
    {
        constexpr ComponentMask delete_mask =
            meta_utils::extract_delete_mask<typename SystemType::EntityQuery>();
        static_assert(
            delete_mask != 0,
            "FATAL: IntentDelete not declared in EntityQuery.");
        db_ref.destroy_entity(id);
    }

    void add_component(EntityId id, ComponentMask mask)
    {
        constexpr ComponentMask add_mask =
            meta_utils::extract_add_mask<typename SystemType::EntityQuery>();
        if((mask & add_mask) != mask)
            throw std::runtime_error(
                "FATAL: System attempted to add undeclared component.");
        db_ref.add_component(id, mask, partition_idx);
    }

    void remove_component(EntityId id, ComponentMask mask)
    {
        constexpr ComponentMask remove_mask =
            meta_utils::extract_remove_mask<typename SystemType::EntityQuery>();
        if((mask & remove_mask) != mask)
            throw std::runtime_error(
                "FATAL: System attempted to remove undeclared component.");
        db_ref.remove_component(id, mask, partition_idx);
    }

    void register_edge(
        EntityId source, EntityId target, ComponentMask edge_mask)
    {
        db_ref.get_mutable_layer().queue_edge_registration(
            source, target, edge_mask);
    }

    std::vector<EntityId> get_outbound_edges(EntityId source) const
    {
        return db_ref.get_outbound_edges(source);
    }

    ComponentMask get_dynamic_mask(EntityId id) const
    {
        return db_ref.get_dynamic_mask(id);
    }
};
} // namespace usagi
