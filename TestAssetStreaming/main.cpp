#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <Usagi/Module/Service/Asset/AssetManager.hpp>
#include <Usagi/Module/Service/Asset/AssetSourceFilesystemDirectory.hpp>

using namespace usagi;

void test_access(volatile char *ptr, std::size_t size)
{
    while(size)
    {
        (void)*ptr;
        ++ptr;
        --size;
    }
}

TEST(TestAssetStreaming, RawAssetRequest)
{
    AssetManager am;
    am.add_source(std::make_unique<AssetSourceFilesystemDirectory>("."));

    const std::u8string asset_name(u8"input.png");

    // load_if_unloaded = false, expect a null handle returned

    auto result = am.request_asset(
        AssetPriority::NORMAL,
        false,
        asset_name
    );

    EXPECT_FALSE(result);
    EXPECT_EQ(result.get(), nullptr);

    // load_if_unloaded = true, expect a waitable handle returned

    result = am.request_asset(
        AssetPriority::NORMAL,
        true,
        asset_name
    );

    EXPECT_TRUE(result);
    EXPECT_NE(result.get(), nullptr);
    EXPECT_NE(result->handle(), 0);

    result->wait();

    EXPECT_TRUE(result->ready());

    // Test access to the data

    const auto seg = result->blob();
    EXPECT_GT(seg.length, 0);
    EXPECT_NE(seg.base_address, nullptr);

    EXPECT_NO_FATAL_FAILURE(test_access(
        static_cast<char*>(seg.base_address),
        seg.length
    ));

    // test cached asset access

    auto result2 = am.request_cached_asset(result->handle());

    EXPECT_TRUE(result2->ready());
    const auto seg2 = result2->blob();

    // Test access to the data

    EXPECT_EQ(seg.length, seg2.length);
    EXPECT_EQ(seg.base_address, seg2.base_address);
}
