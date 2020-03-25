#include <gtest/gtest.h>

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#define NDEBUG

#include <Usagi/Module/Service/Asset/AssetManager.hpp>
#include <Usagi/Module/Service/Asset/AssetSourceFilesystem.hpp>
#include <Usagi/Module/Common/Config/AssetBuilderYamlObject.hpp>

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
