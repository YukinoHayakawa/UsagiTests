#include "Common.hpp"

#include <Usagi/Modules/Resources/ResGraphicsVulkan/RbVulkanShaderModule.hpp>

TEST_F(VulkanTest, ResLoadShaderModule)
{
    [[maybe_unused]]
    auto shader = mHeapManager.resource<RbVulkanShaderModule>(
        { },
        &mExecutor,
        [] {
            static auto gpu_shader_stage = GpuShaderStage::VERTEX;
            return std::forward_as_tuple(
                "shader.vert",
                gpu_shader_stage // bug don't forward_as_tuple rvalue!
            );
        }
    ).make_request().await();
}
