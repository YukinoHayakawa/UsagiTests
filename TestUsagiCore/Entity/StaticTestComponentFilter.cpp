#include <type_traits>

#include <Usagi/Entity/detail/ComponentFilter.hpp>

namespace usagi
{
namespace
{
struct ComponentA
{
};

struct ComponentB
{
};

struct ComponentC
{
};

struct ComponentD
{
};

template <typename... Components>
auto make_filter(Components &&... components)
{
    return ComponentFilter { std::forward<Components>(components)... };
}
}

static_assert(std::is_same_v<
    UniqueComponents<ComponentA, ComponentA, ComponentA>,
    ComponentFilter<ComponentA>
>);

static_assert(std::is_same_v<
    FilterConcatenatedT<
        ComponentFilter<ComponentA, ComponentB>,
        ComponentFilter<ComponentA, ComponentC>
    >,
    ComponentFilter<ComponentA, ComponentB, ComponentA, ComponentC>
>);

static_assert(std::is_same_v<
    FilterDeduplicatedT<FilterConcatenatedT<
        ComponentFilter<ComponentA, ComponentB>,
        ComponentFilter<ComponentD, ComponentA, ComponentC>
    >>,
    ComponentFilter<ComponentA, ComponentB, ComponentD, ComponentC>
>);

static_assert(std::is_same_v<
    decltype(make_filter(ComponentA())),
    ComponentFilter<ComponentA>
>);

static_assert(std::is_same_v<
    decltype(ComponentFilter { ComponentA() }),
    ComponentFilter<ComponentA>
>);

static_assert(std::is_same_v<
    decltype(make_filter()),
    ComponentFilter<>
>);

static_assert(std::is_same_v<
    decltype(ComponentFilter { }),
    ComponentFilter<>
>);
}
