#include <Usagi/Library/Meta/Deduplicate.hpp>

using namespace usagi::meta;

static_assert(std::is_same_v<Deduplicated<int>, Set<int>>);
static_assert(std::is_same_v<Deduplicated<int, int>, Set<int>>);
static_assert(std::is_same_v<Deduplicated<int, double, int>, Set<int, double>>);
