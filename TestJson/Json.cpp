﻿#include <gtest/gtest.h>

#include <Usagi/Modules/Resources/ResJson/RbCascadingJsonConfig.hpp>
#include <Usagi/Modules/Resources/ResJson/RbJsonDocument.hpp>
#include <Usagi/Modules/Runtime/Asset/HeapAssetManager.hpp>
#include <Usagi/Modules/Runtime/Asset/Package/AssetPackageFilesystem.hpp>
#include <Usagi/Modules/Runtime/Executive/ServiceAsyncWorker.hpp>
#include <Usagi/Modules/Runtime/HeapManager/HeapFreeObjectManager.hpp>
#include <Usagi/Modules/Runtime/HeapManager/HeapManager.hpp>

using namespace usagi;

TEST(Json, JsonLoadTest)
{
    HeapManager heap_manager;
    StdTaskExecutor executor;

    auto *asset_manager = heap_manager.add_heap<HeapAssetManager>();
    asset_manager->add_package(std::make_unique<AssetPackageFilesystem>("."));
    heap_manager.add_heap<HeapFreeObjectManager>();

    auto accessor = heap_manager.resource<RbJsonDocument>(
        { },
        &executor,
        [] { return std::forward_as_tuple("config.json"); }
    ).make_request();

    const auto &doc = accessor.await();

    EXPECT_TRUE(doc.root.contains("hello"));
}

TEST(Json, JsonOverrideTest)
{
    HeapManager heap_manager;
    StdTaskExecutor executor;

    auto *asset_manager = heap_manager.add_heap<HeapAssetManager>();
    asset_manager->add_package(std::make_unique<AssetPackageFilesystem>("."));
    heap_manager.add_heap<HeapFreeObjectManager>();

    auto accessor = heap_manager.resource<RbCascadingJsonConfig>(
        { },
        &executor,
        [] { return std::forward_as_tuple("config.json"); }
    ).make_request();

    const auto &doc = accessor.await();

    EXPECT_TRUE(doc.root.contains("world"));
    EXPECT_TRUE(doc.root.contains("base"));
    EXPECT_FALSE(doc.root.contains("inherit"));
    EXPECT_EQ(doc.root["world"].get<bool>(), true);
}
