#include <type_traits>

#include <Usagi/Game/detail/ComponentFilter.hpp>

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
}
