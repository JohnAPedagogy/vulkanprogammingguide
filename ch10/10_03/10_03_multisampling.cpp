#include "my_vulkan.h"
#include <iostream>


int main()
{
    std::cout << "Starting Vulkan Multisampling (Ch10.3) ...\n";
    my_init_vulkan();
    if(device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 0;
    }

    int active_device_index = 0;
    my_get_logical_device(active_device_index);
    my_create_msaa_target();

    // The lesson demonstrates:
    // - Pipeline multisample state with rasterizationSamples = VK_SAMPLE_COUNT_4_BIT
    // - sampleShadingEnable = VK_FALSE
    // - Render pass with two attachments:
    //   * MSAA attachment (VK_SAMPLE_COUNT_4_BIT, load CLEAR, store DONT_CARE)
    //   * Resolve attachment (VK_SAMPLE_COUNT_1_BIT, load DONT_CARE, store STORE)
    // - Subpass with resolve reference pointing from MSAA to resolve
    // - Render pass automatically resolves 4 samples to 1 on vkCmdEndRenderPass
    //
    // In a complete implementation, you would:
    // 1. Create graphics pipeline with the multisample state above
    // 2. Record command buffer with vkCmdBeginRenderPass, vkCmdBindPipeline, vkCmdDraw
    // 3. Submit to queue and wait
    // 4. Read back resolved image and single-sample reference to measure edge smoothing
    // 5. Compare edge metric: MSAA should have lower jagged-edge metric than single-sample
    //
    // The critical path is: MSAA pipeline state → resolve attachment → resolve operation → edge verification

    vk_cleanup();
    return 0;
}
