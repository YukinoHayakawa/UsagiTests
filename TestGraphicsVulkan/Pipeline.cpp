#include "Common.hpp"

#include <Usagi/Modules/Resources/ResGraphicsVulkan/RbVulkanGraphicsPipeline.hpp>

TEST_F(VulkanTest, ResLoadGraphicsPipeline)
{
    [[maybe_unused]]
    auto &pipeline = mHeapManager.resource<RbVulkanGraphicsPipeline>(
        { },
        &mExecutor,
        [] {
            return std::forward_as_tuple("pipeline_1.json");
        }
    ).make_request().await();
    EXPECT_NE(pipeline.pipeline(), vk::Pipeline());
}
