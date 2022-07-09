#include <fstream>
#include <gtest/gtest.h>
#include <Usagi/Modules/Resources/ResCommonImages/RbAssetImage.hpp>

#include <Usagi/Modules/Resources/ResScratchBuffer/HeapCircularBuffer.hpp>
#include <Usagi/Modules/Runtime/Asset/AssetManager2.hpp>
#include <Usagi/Modules/Runtime/Asset/Package/AssetPackageFilesystem.hpp>
#include <Usagi/Modules/Runtime/Executive/ServiceAsyncWorker.hpp>
#include <Usagi/Modules/Runtime/HeapManager/HeapManager.hpp>

using namespace usagi;

class CommonImageTest : public testing::Test {
protected:
    void SetUp() override {
        auto *asset_manager = mHeapManager.add_heap<AssetManager2>();
        asset_manager->add_package(std::make_unique<AssetPackageFilesystem>("."));

        // allocate 256 MiB
        mHeapManager.add_heap<HeapCircularBuffer>(1024 * 1024 * 256);
    }

    HeapManager mHeapManager;
    StdTaskExecutor mExecutor;
};


TEST_F(CommonImageTest, WebPLoading)
{
    [[maybe_unused]]
    auto image = mHeapManager.resource<RbAssetImage>(
        { },
        &mExecutor,
        [] {
            return std::forward_as_tuple(
                "example.webp"
            );
        }
    ).make_request().await();
    std::ofstream ppm { "converted.ppm" };
    ppm << "P3\n" << image->width() << ' ' << image->height() << "\n255\n";
    for(auto y = 0; y < image->height(); ++y)
    {
        for(auto x = 0; x < image->width(); ++x)
        {
            const auto &pixel = image->at({ x, y });
            ppm << 
                (int)pixel.x() << " " << 
                (int)pixel.y() << " " << 
                (int)pixel.z() << '\n';
        }
    }
}
