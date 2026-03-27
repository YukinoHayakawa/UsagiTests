#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <meta> // C++26 P2996R13
#include <vector>

namespace usagi
{
// -----------------------------------------------------------------------------
// Core Mathematical Primitives & Semantics
// -----------------------------------------------------------------------------
using EntityId = uint32_t;
[[maybe_unused]]
constexpr EntityId INVALID_ENTITY       = 0xFFFF'FFFF;
constexpr uint32_t MAX_SPAWN_PARTITIONS = 256;

/* Shio: We represent the existence of a component as a single bit in a 64-bit
   integer. This maps the infinite sparse matrix columns into manageable
   discrete bitwise operations for the Task Graph's intersection tests. */
using ComponentMask = uint64_t;

/**
 * DataAccessFlags defines the explicit memory pipeline semantics for Task Graph
 * Systems.
 * * Purpose: A naive DAG compiler assumes any Read-Write or Write-Write
 * intersection is a fatal data race requiring strict topological serialization.
 * This creates artificial bottlenecks. DataAccessFlags act as mathematically
 * proven edge-severing axioms. They instruct the Executive to safely dissolve
 * specific dependencies (WAW, WAR, RAW) by mapping the logical intent to
 * physical hardware guarantees (e.g., L2 cache atomics, multi-buffered pipeline
 * frames, or DMA transfers).
 */
enum class DataAccessFlags : uint16_t
{
    None = 0,

    /* --- Core Access Semantics --- */
    Read      = 1 << 0,
    Write     = 1 << 1,
    ReadWrite = Read | Write,

    /* --- Physical Memory Topology --- */
    /* * Dictates the required hardware residence of the chunk.
     * If a chunk is resident in VRAM, but a System specifies `Host | Read`,
     * the DAG compiler injects an implicit asynchronous
     * `SystemTransferDeviceToHost` node before this System's execution edge.
     */
    Host   = 1 << 2,
    Device = 1 << 3,

    /* --- Topological Edge Severing (WAW, RAW, WAR) --- */

    /* * Discard (Severs WAW stalls)
     * When combined with Write, the System mathematically guarantees it will
     * overwrite the ENTIRE chunk without reading its prior state. The ECS
     * allocator provisions a newly aliased block of physical memory, completely
     * severing the Write-After-Write dependency. The previous writer proceeds
     * in parallel on the old memory block.
     */
    Discard = 1 << 4,

    /* * Previous (Severs WAR and RAW stalls)
     * Forces the System to read from the Frame N-1 physical memory buffer.
     * - Severs WAR: A reader looking at N-1 does not care if a parallel writer
     * mutates N.
     * - Severs RAW: A reader looking at N-1 does not need to wait for the
     * writer generating N. Essential for physics interpolation, temporal
     * anti-aliasing, and pipelined logic.
     */
    Previous = 1 << 5,

    /* * Atomic (Severs WAW stalls)
     * Declares the System's writes are mathematically isolated via hardware
     * atomics (e.g., lock `xadd`, Vulkan `atomicAdd`). If multiple Systems
     * specify `Write | Atomic` on the same ComponentGroup, the DAG compiler
     * severs the WAW edges between them, allowing massive data-parallel
     * accumulation across the thread pool.
     */
    Atomic = 1 << 6,

    /* * Accumulate (Implicit Reduction Map-Reduce)
     * Data-parallel tasks write to thread-local transient chunks. The DAG
     * compiler injects a deterministic reduction node to merge these local
     * chunks into the global ECS state before the topological barrier resolves.
     */
    Accumulate = 1 << 7,

    // Note: Deferred is mathematically deprecated for Entity Creation to
    // preserve strictly ordered, data-parallel partitioning and total
    // determinism.
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
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = true;
    static constexpr bool          is_write  = false;
    static constexpr bool          is_delete = false;
    static constexpr bool          is_policy = false;
};

template <typename... Ts>
struct Write
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = false;
    static constexpr bool          is_write  = true;
    static constexpr bool          is_delete = false;
    static constexpr bool          is_policy = false;
};

template <typename... Ts>
struct IntentDelete
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = false;
    static constexpr bool          is_write  = false;
    static constexpr bool          is_delete = true;
    static constexpr bool          is_policy = false;
};

/* Yukino: Structural mutation intents. By setting is_write = true, the
   topological compiler automatically maps an IntentAdd/Remove<T> as an
   exclusive Write lock on T. This perfectly prevents data races on the
   component memory chunks without altering the underlying DAG collision solver.
 */
template <typename... Ts>
struct IntentAdd
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = false;
    static constexpr bool          is_write  = true;
    static constexpr bool          is_delete = false;
    static constexpr bool          is_add    = true;
    static constexpr bool          is_policy = false;
};

template <typename... Ts>
struct IntentRemove
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = false;
    static constexpr bool          is_write  = true;
    static constexpr bool          is_delete = false;
    static constexpr bool          is_remove = true;
    static constexpr bool          is_policy = false;
};

template <DataAccessFlags Flags>
struct AccessPolicy
{
    static constexpr DataAccessFlags flags     = Flags;
    static constexpr bool            is_read   = false;
    static constexpr bool            is_write  = false;
    static constexpr bool            is_delete = false;
    static constexpr bool            is_policy = true;
};

template <typename... Args>
struct EntityQuery
{
    /* Yukino: Using P2996 ^^ operator to reflect type arguments into a meta
     * info array. This array is processed at compile-time to enforce access
     * boundaries. */
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
        if constexpr(Arg::is_delete) m |= Arg::mask;
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
// Entity Database
// -----------------------------------------------------------------------------
class EntityDatabase
{
    struct EntityRecord
    {
        EntityId                   id;
        std::atomic<ComponentMask> mask;
        bool                       alive;

        EntityRecord(EntityId i, ComponentMask m, bool a)
            : id(i), mask(m), alive(a)
        {
        }

        EntityRecord(const EntityRecord &)            = delete;
        EntityRecord &operator=(const EntityRecord &) = delete;

        // Required for std::vector reallocation
        EntityRecord(EntityRecord &&other) noexcept
            : id(other.id)
            , mask(other.mask.load(std::memory_order_relaxed))
            , alive(other.alive)
        {
        }
    };

    std::vector<EntityRecord> entities;
    EntityId                  next_id = 1;

    /* Yukino: Partitioned orthogonal staging arrays. Data-parallel systems
       write to deterministic slices (e.g., Workgroup 0 writes to Partition 0)
       instead of a single mutex-locked queue. Preserves Amdahl's scaling and
       guarantees deterministic bit-for-bit ID assignment regardless of thread
       scheduling. */
    std::array<std::vector<ComponentMask>, MAX_SPAWN_PARTITIONS>
                      spawn_partitions;
    std::atomic<bool> has_spawns { false };

public:
    EntityDatabase() = default;

    EntityId create_entity_immediate(ComponentMask initial_mask)
    {
        // Yukino: The Singularity Boundary.
        // A pure 32-bit monotonic counter without a generational recycling
        // scheme cannot violate the Pigeonhole Principle. Wrapping to 1
        // destroys the bijection and causes silent aliasing of active memory.
        // We must mathematically halt.
        if(next_id == INVALID_ENTITY)
        {
            throw std::runtime_error(
                "FATAL: EntityId space exhausted (The 2^32 Singularity).");
        }

        EntityId id = next_id++;
        entities.emplace_back(id, initial_mask, true);
        return id;
    }

    void destroy_entity_immediate(EntityId id)
    {
        for(auto &rec : entities)
        {
            if(rec.id == id)
            {
                rec.alive = false;
                // Obliterate structural footprint
                rec.mask.store(0, std::memory_order_release);
                return;
            }
        }
    }

    /* Shio: The query execution. Used by both Systems during logic, and the
       Executive during the JIT pre-pass. */
    std::vector<EntityId> query_entities(ComponentMask required_mask) const
    {
        std::vector<EntityId> results;
        for(const auto &rec : entities)
        {
            // Shio: Safe concurrent query against structural mutation via
            // acquire barrier.
            ComponentMask current_mask =
                rec.mask.load(std::memory_order_acquire);
            if(rec.alive &&
                (required_mask == 0 ||
                    (current_mask & required_mask) == required_mask))
            {
                results.push_back(rec.id);
            }
        }
        return results;
    }

    ComponentMask get_dynamic_mask(EntityId id) const
    {
        for(const auto &rec : entities)
        {
            if(rec.id == id && rec.alive)
                return rec.mask.load(std::memory_order_acquire);
        }
        return 0;
    }

    /* Yukino: Modifying an entity's structure at runtime. This simulates an
       entity joining a new ComponentGroup dynamically. */
    void add_component_immediate(EntityId id, ComponentMask comp_mask)
    {
        for(auto &rec : entities)
        {
            if(rec.id == id && rec.alive)
            {
                rec.mask.fetch_or(comp_mask, std::memory_order_release);
                return;
            }
        }
    }

    void remove_component_immediate(EntityId id, ComponentMask comp_mask)
    {
        for(auto &rec : entities)
        {
            if(rec.id == id && rec.alive)
            {
                rec.mask.fetch_and(~comp_mask, std::memory_order_release);
                return;
            }
        }
    }

    // --- Re-entrancy specific ---
    void queue_spawn(ComponentMask initial_mask, uint32_t partition_id)
    {
        // Data-parallel safe: Threads writing to disparate partition IDs do not
        // contend.
        spawn_partitions[partition_id].push_back(initial_mask);
        has_spawns.store(true, std::memory_order_relaxed);
    }

    bool has_pending_spawns() const
    {
        return has_spawns.load(std::memory_order_relaxed);
    }

    void commit_pending_spawns()
    {
        if(!has_spawns.load(std::memory_order_relaxed)) return;

        // Shio: Flattened sequentially. The deterministic traversal order
        // guarantees identical Entity IDs across infinite simulation runs.
        for(auto &partition : spawn_partitions)
        {
            for(auto mask : partition)
            {
                create_entity_immediate(mask);
            }
            partition.clear();
        }
        has_spawns.store(false, std::memory_order_relaxed);
    }

    void clear_database()
    {
        entities.clear();
        for(auto &p : spawn_partitions)
            p.clear();
        has_spawns.store(false, std::memory_order_relaxed);
        next_id = 1;
    }

    // Shio: Singularity injection hook for unit testing asymptotic limits.
    void debug_set_next_id(EntityId id) { next_id = id; }
};

// -----------------------------------------------------------------------------
// Compile-Time Capability Proxy (DatabaseAccess)
// -----------------------------------------------------------------------------
/* Shio: This proxy acts as a mathematical firewall. It prevents Systems from
   executing structural changes they did not explicitly request in EntityQuery.
 */
template <typename SystemType>
class DatabaseAccess
{
    EntityDatabase &db_ref;
    uint32_t        partition_idx;
    using Query = typename SystemType::EntityQuery;

public:
    explicit DatabaseAccess(EntityDatabase &db, uint32_t partition = 0)
        : db_ref(db), partition_idx(partition)
    {
    }

    std::vector<EntityId> query_entities(ComponentMask mask) const
    {
        return db_ref.query_entities(mask);
    }

    void queue_spawn(ComponentMask initial_mask)
    {
        db_ref.queue_spawn(initial_mask, partition_idx);
    }

    void destroy_entity(EntityId id)
    {
        constexpr ComponentMask delete_mask =
            meta_utils::extract_delete_mask<Query>();
        static_assert(
            delete_mask != 0,
            "FATAL: System attempted to delete an entity, but IntentDelete was "
            "not declared in EntityQuery.");
        db_ref.destroy_entity_immediate(id);
    }

    void add_component(EntityId id, ComponentMask mask)
    {
        constexpr ComponentMask add_mask =
            meta_utils::extract_add_mask<Query>();
        if((mask & add_mask) != mask)
        {
            throw std::runtime_error(
                "FATAL: System attempted to add undeclared component.");
        }
        db_ref.add_component_immediate(id, mask);
    }

    void remove_component(EntityId id, ComponentMask mask)
    {
        constexpr ComponentMask remove_mask =
            meta_utils::extract_remove_mask<Query>();
        if((mask & remove_mask) != mask)
        {
            throw std::runtime_error(
                "FATAL: System attempted to remove undeclared component.");
        }
        db_ref.remove_component_immediate(id, mask);
    }
};
} // namespace usagi
