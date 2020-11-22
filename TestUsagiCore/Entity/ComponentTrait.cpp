#include <Usagi/Entity/detail/ComponentTrait.hpp>

using namespace usagi;

struct A
{
    using TagComponent = void;
};
static_assert(TagComponent<A>);

struct B
{
};
static_assert(!TagComponent<B>);

USAGI_DECL_TAG_COMPONENT(C);
static_assert(TagComponent<C>);
