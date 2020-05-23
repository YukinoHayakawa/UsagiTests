#include <Usagi/Game/detail/ComponentAccessSystemAttribute.hpp>
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

struct System1
{
    using ReadAccess = ComponentFilter<ComponentA>;
    using WriteAccess = ComponentFilter<ComponentA, ComponentB>;
};
static_assert(System<System1>);
static_assert(SystemDeclaresReadAccess<System1>);

struct System2
{
    using ReadAllAccess = void;
    using WriteAccess = ComponentFilter<ComponentA, ComponentB, ComponentC>;
};

struct System3
{
    using ReadAccess = ComponentFilter<ComponentC, ComponentA, ComponentD>;
    using WriteAllAccess = void;
};

static_assert(std::is_same_v<
    SystemComponentUsage<System1, System2, System3>,
    ComponentFilter<ComponentA, ComponentB, ComponentC, ComponentD>
>);
}
}
