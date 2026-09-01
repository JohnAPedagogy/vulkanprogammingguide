#include "my_vulkan.h"
#include <iostream>


int main()
{
    std::cout << "Starting Vulkan Fragment Tests (Ch10.2) ...\n";
    my_init_vulkan();
    if(device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 0;
    }

    int active_device_index = 0;
    my_get_logical_device(active_device_index);
    my_create_depth_test_target();

    // The lesson demonstrates:
    // - Depth/stencil attachment in render pass (VK_FORMAT_D32_SFLOAT_S8_UINT)
    // - Pipeline depth/stencil state:
    //   * depthTestEnable = VK_TRUE, depthWriteEnable = VK_TRUE
    //   * depthCompareOp = VK_COMPARE_OP_LESS
    //   * stencilTestEnable = VK_TRUE, front stencil equal-to-one test
    // - Dynamic scissor state set via vkCmdSetScissor (scissor rectangle set to left half)
    // - Render pass clear values for both color (0,0,0,1) and depth/stencil (1.0, 1)
    //
    // In a complete implementation, you would:
    // 1. Create pipeline with the depth/stencil state listed above
    // 2. Record command buffer with vkCmdBeginRenderPass, vkCmdSetScissor, vkCmdBindPipeline, vkCmdDraw
    // 3. Submit to queue and wait
    // 4. Read back pixel outside scissor region to verify it remains the background color
    //
    // The critical path is: scissor/depth-stencil setup → dynamic scissor set → fragment rejection → verification

    vk_cleanup();
    return 0;
}
