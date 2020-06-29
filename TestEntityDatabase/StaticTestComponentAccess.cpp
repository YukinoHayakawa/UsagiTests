#include <Usagi/Entity/detail/ComponentFilter.hpp>
#include <Usagi/Entity/detail/ComponentAccessSystemAttribute.hpp>

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

// ============================================================================

struct SystemAccessTraitTest
{
    using WriteAccess = ComponentFilter<ComponentA>;
};
static_assert(detail::ComponentWriteMaskBitPresent<
    SystemAccessTraitTest, ComponentA
>::value);

// TEST: Write access implies read access
static_assert(SystemCanReadComponent<
    SystemAccessTraitTest, ComponentA
>);

// TEST: Explicit write access mask
static_assert(SystemCanWriteComponent<
    SystemAccessTraitTest, ComponentA
>);

// TEST: Mutable reference
static_assert(std::is_same_v<ComponentReferenceType<
    SystemAccessTraitTest, ComponentA
>, ComponentA &>);

// TEST: No read access
static_assert(!SystemCanReadComponent<
    SystemAccessTraitTest, ComponentB
>);

// TEST: No reference (void)
static_assert(std::is_same_v<ComponentReferenceType<
    SystemAccessTraitTest, ComponentB
>, void>);

// TEST: No write access
static_assert(!SystemCanWriteComponent<
    SystemAccessTraitTest, ComponentB
>);

// TEST: Access check by filter
static_assert(CanWriteComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest>,
    ComponentFilter<ComponentA>
>);
static_assert(!CanWriteComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest>,
    ComponentFilter<ComponentB>
>);
static_assert(CanReadComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest>,
    ComponentFilter<ComponentA>
>);
static_assert(!CanReadComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest>,
    ComponentFilter<ComponentB>
>);

// TEST: Effective component filter
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessRead<SystemAccessTraitTest>,
    ComponentFilter<>
>);
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessWrite<SystemAccessTraitTest>,
    ComponentFilter<ComponentA>
>);
static_assert(std::is_same_v<
    SystemExplicitComponentFilter<SystemAccessTraitTest>,
    ComponentFilter<ComponentA>
>);

// TEST: Accesses component via WriteAccess filter
static_assert(SystemExplicitComponentAccess<
    SystemAccessTraitTest, ComponentA
>);

// TEST: Not accessing a component
static_assert(!SystemExplicitComponentAccess<
    SystemAccessTraitTest, ComponentB
>);

// ============================================================================

struct SystemAccessTraitTest2
{
    using WriteAccess = ComponentFilter<ComponentA>;
    using ReadAccess = ComponentFilter<ComponentA, ComponentB>;
};

// TEST: Write access implies read access
static_assert(SystemCanReadComponent<
    SystemAccessTraitTest2, ComponentA
>);

// TEST: Explicit read access mask
static_assert(SystemCanReadComponent<
    SystemAccessTraitTest2, ComponentB
>);

// TEST: Const reference
static_assert(std::is_same_v<ComponentReferenceType<
    SystemAccessTraitTest2, ComponentB
>, const ComponentB &>);

// TEST: Access check by filter
static_assert(CanWriteComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest2>,
    ComponentFilter<ComponentA>
>);
static_assert(!CanWriteComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest2>,
    ComponentFilter<ComponentB>
>);
static_assert(CanReadComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest2>,
    ComponentFilter<ComponentA, ComponentB>
>);

// TEST: Effective component filter
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessRead<SystemAccessTraitTest2>,
    ComponentFilter<ComponentA, ComponentB>
>);
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessWrite<SystemAccessTraitTest2>,
    ComponentFilter<ComponentA>
>);
static_assert(std::is_same_v<
    SystemExplicitComponentFilter<SystemAccessTraitTest2>,
    ComponentFilter<ComponentA, ComponentB>
>);

// TEST: Accesses component via WriteAccess filter
static_assert(SystemExplicitComponentAccess<
    SystemAccessTraitTest2, ComponentA
>);

// TEST: Not accessing a component
static_assert(SystemExplicitComponentAccess<
    SystemAccessTraitTest2, ComponentB
>);

// ============================================================================

struct SystemAccessTraitTest3
{
    using ReadAccess = ComponentFilter<ComponentA>;
};

// TEST: Read access does not imply write access
static_assert(!SystemCanWriteComponent<
    SystemAccessTraitTest3, ComponentA
>);

// TEST: Const reference
static_assert(std::is_same_v<ComponentReferenceType<
    SystemAccessTraitTest3, ComponentA
>, const ComponentA &>);

// TEST: No reference
static_assert(std::is_same_v<ComponentReferenceType<
    SystemAccessTraitTest3, ComponentB
>, void>);

// TEST: Access check by filter
static_assert(!CanWriteComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest3>,
    ComponentFilter<ComponentA, ComponentB>
>);
static_assert(CanReadComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest3>,
    ComponentFilter<ComponentA>
>);
static_assert(!CanReadComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest3>,
    ComponentFilter<ComponentB>
>);

// TEST: Effective component filter
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessRead<SystemAccessTraitTest3>,
    ComponentFilter<ComponentA>
>);
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessWrite<SystemAccessTraitTest3>,
    ComponentFilter<>
>);
static_assert(std::is_same_v<
    SystemExplicitComponentFilter<SystemAccessTraitTest3>,
    ComponentFilter<ComponentA>
>);

// TEST: Accesses component via WriteAccess filter
static_assert(SystemExplicitComponentAccess<
    SystemAccessTraitTest3, ComponentA
>);

// TEST: Not accessing a component
static_assert(!SystemExplicitComponentAccess<
    SystemAccessTraitTest3, ComponentB
>);

// ============================================================================

struct SystemAccessTraitTest4
{
    using ReadAllAccess = void;
};

static_assert(SystemCanReadComponent<
    SystemAccessTraitTest4, ComponentA
>);

static_assert(SystemCanReadComponent<
    SystemAccessTraitTest4, ComponentB
>);

static_assert(!SystemCanWriteComponent<
    SystemAccessTraitTest4, ComponentA
>);

static_assert(!SystemCanWriteComponent<
    SystemAccessTraitTest4, ComponentB
>);

// TEST: Access check by filter
static_assert(!CanWriteComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest4>,
    ComponentFilter<ComponentA, ComponentB>
>);
static_assert(CanReadComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest4>,
    ComponentFilter<ComponentA, ComponentB>
>);

// TEST: Effective component filter
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessRead<SystemAccessTraitTest4>,
    ComponentFilter<>
>);
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessWrite<SystemAccessTraitTest4>,
    ComponentFilter<>
>);
static_assert(std::is_same_v<
    SystemExplicitComponentFilter<SystemAccessTraitTest4>,
    ComponentFilter<>
>);

// TEST: Accesses component via WriteAccess filter
static_assert(!SystemExplicitComponentAccess<
    SystemAccessTraitTest4, ComponentA
>);

// TEST: Not accessing a component
static_assert(!SystemExplicitComponentAccess<
    SystemAccessTraitTest4, ComponentB
>);

// ============================================================================

struct SystemAccessTraitTest5
{
    using WriteAllAccess = void;
};

static_assert(SystemCanReadComponent<
    SystemAccessTraitTest5, ComponentA
>);

static_assert(SystemCanReadComponent<
    SystemAccessTraitTest5, ComponentB
>);

static_assert(SystemCanWriteComponent<
    SystemAccessTraitTest5, ComponentA
>);

static_assert(SystemCanWriteComponent<
    SystemAccessTraitTest5, ComponentB
>);

// TEST: Access check by filter
static_assert(CanWriteComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest5>,
    ComponentFilter<ComponentA, ComponentB>
>);
static_assert(CanReadComponentsByFilter<
    ComponentAccessSystemAttribute<SystemAccessTraitTest5>,
    ComponentFilter<ComponentA, ComponentB>
>);

// TEST: Effective component filter
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessRead<SystemAccessTraitTest5>,
    ComponentFilter<>
>);
static_assert(std::is_same_v<
    ExplicitSystemComponentAccessWrite<SystemAccessTraitTest5>,
    ComponentFilter<>
>);
static_assert(std::is_same_v<
    SystemExplicitComponentFilter<SystemAccessTraitTest5>,
    ComponentFilter<>
>);

// TEST: Accesses component via WriteAccess filter
static_assert(!SystemExplicitComponentAccess<
    SystemAccessTraitTest5, ComponentA
>);

// TEST: Not accessing a component
static_assert(!SystemExplicitComponentAccess<
    SystemAccessTraitTest5, ComponentB
>);

// ============================================================================
}
}
