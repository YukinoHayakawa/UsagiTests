// #include <random>
#include <gtest/gtest.h>

#include <Usagi/Modules/Resources/ResWindowManager/RbNativeWindow.hpp>
// #include <Usagi/Modules/Runtime/HeapManager/HeapFreeObjectManager.hpp>
#include <Usagi/Modules/Runtime/HeapManager/HeapManager.hpp>
// #include <Usagi/Modules/Runtime/HeapManager/HeapManagerStatic.hpp>

#ifdef _WIN32
#include <Usagi/Modules/Platforms/WinCommon/Windowing/NativeWindowWin32.hpp>
#endif

using namespace usagi;

class WindowingTest : public ::testing::Test
{
protected:
    // void TearDown() override {}

    // StdTaskExecutor mExecutor;
    void SetUp() override
    {
        // auto *asset_manager = mHeapManager.add_heap<HeapAssetManager>();
        // asset_manager->add_package(std::make_unique<AssetPackageFilesystem>("."));

        // mHeapManager.add_heap<HeapFreeObjectManager>();

        mHeapManager.add_heap(NativeWindowManager::create_native_manager());
    }

    /*
    // heap objects may refer to gpu device, put at the end
    HeapManagerStatic<
        // HeapFreeObjectManager,
        // HeapWindowManager

    > mHeapManager {
        // std::forward_as_tuple(),
        std::forward_as_tuple(),
    };
    */

    HeapManager mHeapManager;
};

static_assert(ResourceBuilder<RbNativeWindow>);

TEST_F(WindowingTest, WindowCreationTest)
{
    // std::random_device dev;
    // std::mt19937 rng(dev());
    // std::uniform_int_distribution<std::uint64_t> dist(0, -1);
    [[maybe_unused]]
    auto window = mHeapManager.resource_transient<RbNativeWindow>(
        // dist(rng),
        "test",
        "HelloFromUsagiEngine",
        Vector2f(500, 500),
        Vector2f(1280, 720),
        1,
        NativeWindowState::NORMAL
    ).await();

#ifdef _WIN32
    EXPECT_NE(FindWindowW(nullptr, L"HelloFromUsagiEngine"), HWND());
#endif
}
