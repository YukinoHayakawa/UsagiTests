﻿#ifdef _DEBUG
#pragma comment(lib, "gtestd.lib")
#else
#pragma comment(lib, "gtest.lib")
#endif

#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#define NDEBUG

#include <Usagi/Modules/Services/Asset/AssetManager.hpp>
#include <Usagi/Modules/Services/Asset/AssetSourceFilesystem.hpp>
#include <Usagi/Modules/Common/Config/AssetBuilderYamlObject.hpp>

using namespace usagi;

TEST(TestModuleConfig, YamlParse)
{
    AssetManager asset_manager;
    asset_manager.add_source(std::make_unique<AssetSourceFilesystem>("."));

    auto config = asset_manager.request_asset<AssetBuilderYamlObject>(
        AssetPriority::NORMAL,
        true,
        u8"pipeline.yaml"
    );
    config->wait();

    auto &node = config->cast<AssetBuilderYamlObject::OutputT>();

    auto view = node["vertex-shader"]["shader"].val();
    EXPECT_EQ(
        std::string_view(view.data(), view.size()),
        std::string_view("shader.vert")
    );
}
