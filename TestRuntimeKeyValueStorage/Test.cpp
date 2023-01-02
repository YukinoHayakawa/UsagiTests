

#include <gtest/gtest.h>

#include <Usagi/Modules/Runtime/KeyValueStorage/OperatorReadRuntimeKeyValue.hpp>
#include <Usagi/Modules/Runtime/KeyValueStorage/ServiceRuntimeKeyValueStorage.hpp>

using namespace usagi;

struct Runtime
    : ServiceRuntimeKeyValueStorage
{
};

TEST(RuntimeKeyValueStorage, OpReadTest)
{
    Runtime rt;
    ServiceAccess<ServiceRuntimeKeyValueStorage> access(rt);

    constexpr std::size_t val = 1234;
    constexpr auto key = "size";

    access.kv_storage().ensure<std::size_t>(key) = val;

    OperatorReadRuntimeKeyValue<"size", std::size_t> op;
    EXPECT_EQ(op(access, std::nullptr_t()), val);
}
