#pragma once

#include <meta> // C++26 P2996R13
#include <vector>

namespace usagi
{
// -----------------------------------------------------------------------------
// Compile-Time Utilities & C++26 Meta-programming
// -----------------------------------------------------------------------------
template <size_t N>
struct FixedString
{
    char data[N] { };

    constexpr FixedString(const char (&str)[N])
    {
        for(size_t i = 0; i < N - 1; ++i)
            data[i] = str[i];
    }

    constexpr bool operator==(const FixedString<N> &other) const
    {
        for(size_t i = 0; i < N; ++i)
            if(data[i] != other.data[i]) return false;
        return true;
    }
};

using EntityId = uint32_t;
[[maybe_unused]]
constexpr EntityId INVALID_ENTITY = 0xFFFF'FFFF;

/* Shio: We represent the existence of a component as a single bit in a 64-bit
   integer. This maps the infinite sparse matrix columns into manageable
   discrete bitwise operations for the Task Graph's intersection tests. */
using ComponentMask = uint64_t;

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
};

template <typename... Ts>
struct Write
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = false;
    static constexpr bool          is_write  = true;
    static constexpr bool          is_delete = false;
};

template <typename... Ts>
struct IntentDelete
{
    static constexpr ComponentMask mask      = build_mask<Ts...>();
    static constexpr bool          is_read   = false;
    static constexpr bool          is_write  = false;
    static constexpr bool          is_delete = true;
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
} // namespace meta_utils

// -----------------------------------------------------------------------------
// Entity Database
// -----------------------------------------------------------------------------
class EntityDatabase
{
    struct EntityRecord
    {
        EntityId      id;
        ComponentMask mask;
        bool          alive;
    };

    std::vector<EntityRecord> entities;
    EntityId                  next_id = 1;

    // Deferred structural queues for re-entrancy
    std::vector<ComponentMask> spawn_queue;

public:
    EntityDatabase() = default;

    EntityId create_entity_immediate(ComponentMask initial_mask)
    {
        EntityId id = next_id++;
        entities.push_back({ id, initial_mask, true });
        return id;
    }

    void destroy_entity_immediate(EntityId id)
    {
        for(auto &rec : entities)
        {
            if(rec.id == id && rec.alive)
            {
                rec.alive = false;
                rec.mask  = 0; // Obliterate structural footprint
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
            // Shio: A mask of 0 is a mathematical universal set. It matches all
            // active entities.
            if(rec.alive &&
                (required_mask == 0 ||
                    (rec.mask & required_mask) == required_mask))
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
            if(rec.id == id && rec.alive) return rec.mask;
        }
        return 0;
    }

    /* Yukino: Modifying an entity's structure at runtime. This simulates an
       entity joining a new ComponentGroup dynamically. */
    void add_component_immediate(EntityId id, ComponentMask comp_mask)
    {
        for(auto &rec : entities)
        {
            if(rec.id == id && rec.alive) rec.mask |= comp_mask;
        }
    }

    // --- Re-entrancy specific ---
    void queue_spawn(ComponentMask initial_mask)
    {
        spawn_queue.push_back(initial_mask);
    }

    bool has_pending_spawns() const { return !spawn_queue.empty(); }

    void commit_pending_spawns()
    {
        for(auto mask : spawn_queue)
        {
            create_entity_immediate(mask);
        }
        spawn_queue.clear();
    }

    void clear_database()
    {
        entities.clear();
        spawn_queue.clear();
        next_id = 1;
    }
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
    using Query = typename SystemType::EntityQuery;

public:
    explicit DatabaseAccess(EntityDatabase &db)
        : db_ref(db)
    {
    }

    std::vector<EntityId> query_entities(ComponentMask mask) const
    {
        return db_ref.query_entities(mask);
    }

    void queue_spawn(ComponentMask initial_mask)
    {
        db_ref.queue_spawn(initial_mask);
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
};
} // namespace usagi
