#include <Usagi/Modules/Resources/ResGraphicsVulkan/Workload/RbVulkanCommandListGraphics.hpp>

#include "Common.hpp"

TEST_F(VulkanTest, CommandListAllocation)
{
    auto cmd_list = mHeapManager.resource_transient<RbVulkanCommandListGraphics>(
        mHeapManager.make_unique_descriptor()
        // 0, std::type_index(typeid(void)), std::this_thread::get_id()
    ).await();

    (*cmd_list)
        .begin_recording()
        .end_recording();

    auto cmd_submission_list = mDevice.create_command_buffer_list();
    // todo pass ref counted to submission list
    cmd_submission_list.add(std::move(*cmd_list));

    auto wait_sem = mDevice.create_semaphore_info();
    auto signal_sem = mDevice.create_semaphore_info();

    mDevice.submit_graphics_jobs(
        std::move(cmd_submission_list),
        std::move(wait_sem),
        // render finished sem signaled here
        std::move(signal_sem)
    );

    EXPECT_NO_FATAL_FAILURE(mDevice.wait_idle());
}

/*
TEST_F(VulkanTest, CommandListIdentity)
{
    auto cmd_list = mHeapManager.resource_transient<RbVulkanCommandListGraphics>(
        0, std::type_index(typeid(void)), std::this_thread::get_id()
    );

    auto cmd_list2 = mHeapManager.resource_transient<RbVulkanCommandListGraphics>(
        0, std::type_index(typeid(void)), std::this_thread::get_id()
    );

    auto cmd_list3 = mHeapManager.resource_transient<RbVulkanCommandListGraphics>(
        1, std::type_index(typeid(void)), std::this_thread::get_id()
    );

    auto cmd_list4 = mHeapManager.resource_transient<RbVulkanCommandListGraphics>(
        1, std::type_index(typeid(int)), std::this_thread::get_id()
    );

    EXPECT_EQ(cmd_list.descriptor(), cmd_list2.descriptor());
    EXPECT_NE(cmd_list2.descriptor(), cmd_list3.descriptor());
    EXPECT_NE(cmd_list3.descriptor(), cmd_list4.descriptor());
}
*/

// todo immediate transient resource: don't enter taskexecutor & always wait until ready
// todo frame resource recycling - also remove heap mgr record
// todo check frame resource identity
//
// static_assert(ImmediateResource<RbVulkanCommandListGraphics>);
// static_assert(TransientResource<RbVulkanCommandListGraphics>);
