﻿#include <Usagi/Entity/EntityDatabase.hpp>
#include <Usagi/Entity/Archetype.hpp>
#include <Usagi/Entity/detail/ComponentAccessSystemAttribute.hpp>
#include <Usagi/Entity/detail/EntityDatabaseAccessExternal.hpp>
#include <Usagi/Entity/System.hpp>

namespace usagi
{
namespace
{
// ============================================================================

struct ComponentA
{
};

struct ComponentB
{
};

using Database = ComponentFilter<ComponentA, ComponentB>::
    apply<EntityDatabaseDefaultConfig>;

using ArchetypeA = Archetype<ComponentA>;
using ArchetypeB = Archetype<ComponentB>;

struct SystemA
{
    using WriteAccess = ArchetypeA::ComponentFilterT;
};

struct SystemB
{
    using WriteAccess = AllComponents;
};

struct SystemC
{
    using WriteAccess = ComponentFilter<ComponentA, ComponentB>;
};

template <typename T>
using Access = EntityDatabaseAccessExternal<
    Database,
    ComponentAccessSystemAttribute<T>
>;

// ============================================================================

template <typename T>
concept CanCreateA = requires (T)
{
    Access<T>(nullptr).insert(std::declval<ArchetypeA&>());
};

template <typename T>
concept CanCreateB = requires (T)
{
    Access<T>(nullptr).insert(std::declval<ArchetypeB&>());
};

static_assert(CanCreateA<SystemA>);
static_assert(!CanCreateB<SystemA>);

// ============================================================================

template <typename T>
concept CanDestroyEntity = requires (T)
{
    (*Access<T>(nullptr).view().begin()).destroy();
};

static_assert(!CanDestroyEntity<SystemA>);
static_assert(CanDestroyEntity<SystemB>);
static_assert(CanDestroyEntity<SystemC>);

// ============================================================================
}
}
