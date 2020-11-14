#include <Usagi/Entity/detail/ComponentTrait.hpp>

using namespace usagi;

struct A
{
    using FlagComponent = void;
};
static_assert(IsFlagComponent<A>);

struct B
{
};
static_assert(!IsFlagComponent<B>);

USAGI_DECL_FLAG_COMPONENT(C);
static_assert(IsFlagComponent<C>);
