#ifdef _DEBUG
#pragma comment(lib, "gtestd.lib")
#pragma comment(lib, "fmtd.lib")
#else
#pragma comment(lib, "gtest.lib")
#pragma comment(lib, "fmt.lib")
#endif

#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#include <Usagi/Modules/Services/Asset/AssetManager.hpp>
#include <Usagi/Modules/Services/Asset/AssetSourceFilesystem.hpp>

using namespace usagi;

void a(auto b)
{
    decltype(b) c = b;
    (void)c;
}

void b()
{
    a(1);
}

void test_access(volatile char *ptr, std::size_t size)
{
    while(size)
    {
        (void)*ptr;
        ++ptr;
        --size;
    }
}

TEST(RefCountedTest, CopyTest)
{
    std::atomic<uint32_t> counter = 0;

    struct Traits
    {
        static std::uint32_t increment_reference(std::atomic<uint32_t> *entry)
        {
            return ++*entry;
        }

        static std::uint32_t decrement_reference(std::atomic<uint32_t> *entry)
        {
            return --*entry;
        }

        static void free(std::atomic<uint32_t> *entry)
        {
            printf("counter reset to 0!\n");
        }
    };

    using Handle = RefCounted<std::atomic<uint32_t>, Traits>;

    EXPECT_EQ(counter, 0);
    {
        Handle rc(&counter);
        EXPECT_EQ(counter, 1);
        Handle rc2 = rc; // copy
        EXPECT_EQ(counter, 2);
        Handle rc3 = std::move(rc2); // move
        EXPECT_EQ(counter, 2);
        Handle rc4 = std::move(rc); // move
        EXPECT_EQ(counter, 2);
    }
    EXPECT_EQ(counter, 0);
}

TEST(TestAssetStreaming, RawAssetRequest)
{
    AssetManager am;
    am.add_source(std::make_unique<AssetSourceFilesystem>("."));

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

    result->wait();

    EXPECT_NE(result.get(), nullptr);
    EXPECT_TRUE(result->ready());
    EXPECT_NE(result->handle(), 0);
    EXPECT_EQ(result->use_count(), 1);

    // Test access to the data

    const auto seg = result->blob();
    EXPECT_GT(seg.length, 0);
    EXPECT_NE(seg.base_address, nullptr);

    ASSERT_NO_FATAL_FAILURE(test_access(
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

TEST(TestAssetStreaming, MultithreadedRequest)
{
    AssetManager am;
    am.add_source(std::make_unique<AssetSourceFilesystem>("."));

    const std::u8string asset_name(u8"input.png");

    auto result0 = am.request_asset(
        AssetPriority::NORMAL,
        true,
        asset_name
    );
    result0->wait();

    ASSERT_TRUE(result0->ready());

    const auto blob0 = result0->blob();
    EXPECT_EQ(result0->use_count(), 1);

    // Multi-threaded pressure test

    const std::size_t test_size = 1000;
    std::vector<std::future<void>> futures;
    futures.reserve(test_size);

    const auto job = [&](const std::size_t idx) {
        auto result = am.request_asset(
            AssetPriority::NORMAL,
            true,
            asset_name
        );
        EXPECT_GT(result->use_count(), 1u);
        EXPECT_EQ(result->handle(), result0->handle());
        EXPECT_TRUE(result->ready());
        auto b = result->blob();
        EXPECT_EQ(b.base_address, blob0.base_address);
        EXPECT_EQ(b.length, blob0.length);
        // printf("%lu\n", result->use_count());
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
    };

    // launch jobs
    for(auto i = 0; i < test_size; ++i)
    {
        futures.emplace_back(std::async(std::launch::async, job, i));
    }

    // wait all jobs
    for(auto &&f : futures)
    {
        f.wait();
        // printf("%lu\n", result0->use_count());
    }

    EXPECT_EQ(result0->use_count(), 1);
}
