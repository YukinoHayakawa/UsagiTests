#pragma once

#include <gtest/gtest.h>

#include <Usagi/Modules/Platforms/Vulkan/VulkanGpuDevice.hpp>
#include <Usagi/Modules/Runtime/Asset/AssetManager2.hpp>
#include <Usagi/Modules/Runtime/Asset/Package/AssetPackageFilesystem.hpp>
#include <Usagi/Modules/Runtime/Executive/ServiceAsyncWorker.hpp>
#include <Usagi/Modules/Runtime/HeapManager/HeapManager.hpp>

using namespace usagi;

class VulkanTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto *asset_manager = mHeapManager.add_heap<AssetManager2>();
        asset_manager->add_package(std::make_unique<AssetPackageFilesystem>("."));

        // yes, just use pointer type as a heap :)
        mHeapManager.add_heap<VulkanDeviceAccess *>(&mDevice);
        // mHeapManager.add_heap<HeapVulkanObjectManager>(&mDevice);
    }

    // void TearDown() override {}

    VulkanGpuDevice mDevice;
    // heap objects may refer to gpu device, put at the end
    HeapManager mHeapManager;
    StdTaskExecutor mExecutor;
};
