#include "Common.hpp"

#include <Usagi/Modules/Resources/ResGraphicsVulkan/Pipeline/RbVulkanShaderModule.hpp>

TEST_F(VulkanTest, ResLoadShaderModule)
{
    [[maybe_unused]]
    auto shader = mHeapManager.resource<RbVulkanShaderModule>(
        { },
        &mExecutor,
        [] {
            return std::make_tuple(
                "shader.vert",
                GpuShaderStage::VERTEX
            );
        }
    ).make_request().await();
}
