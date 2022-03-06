#pragma once

#include <gtest/gtest.h>

#include <Usagi/Modules/Platforms/Vulkan/VulkanGpuDevice.hpp>
#include <Usagi/Modules/Resources/ResGraphicsVulkan/HeapVulkanObjectManager.hpp>
#include <Usagi/Modules/Runtime/Asset/HeapAssetManager.hpp>
#include <Usagi/Modules/Runtime/Asset/Package/AssetPackageFilesystem.hpp>
#include <Usagi/Modules/Runtime/Executive/ServiceAsyncWorker.hpp>
#include <Usagi/Modules/Runtime/HeapManager/HeapFreeObjectManager.hpp>
#include <Usagi/Modules/Runtime/HeapManager/HeapManager.hpp>

using namespace usagi;

class VulkanTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto *asset_manager = mHeapManager.add_heap<HeapAssetManager>();
        asset_manager->add_package(std::make_unique<AssetPackageFilesystem>("."));

        mHeapManager.add_heap<HeapFreeObjectManager>();
        mHeapManager.add_heap<HeapVulkanObjectManager>(&mDevice);
    }

    // void TearDown() override {}

    StdTaskExecutor mExecutor;
    VulkanGpuDevice mDevice;
    // heap objects may refer to gpu device, put at the end
    HeapManager mHeapManager;
};
