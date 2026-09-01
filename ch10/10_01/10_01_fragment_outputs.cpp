#include "my_vulkan.h"
#include <iostream>


int main()
{
    std::cout << "Starting Vulkan Fragment Outputs (Ch10.1) ...\n";
    my_init_vulkan();
    if(device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 0;
    }

    int active_device_index = 0;
    my_get_logical_device(active_device_index);
    my_create_render_target();

    // The lesson demonstrates:
    // - Fragment shader with two output locations (location 0 for litColor, location 1 for packedNormal)
    // - Two color attachment blend states (both blending disabled, colorWriteMask = 0xF)
    // - Render pass with two color attachments
    // - Framebuffer with two color attachment images
    //
    // In a complete implementation, you would:
    // 1. Compile fragment shader with SPIR-V (two output locations)
    // 2. Create graphics pipeline with blend attachment state for 2 attachments
    // 3. Record command buffer with vkCmdBeginRenderPass, vkCmdBindPipeline, vkCmdDraw
    // 4. Submit to queue and wait for completion
    // 5. Read back pixels from both attachments to verify output
    //
    // The critical path is: fragment outputs → blend attachment count match → attachment verification

    vk_cleanup();
    return 0;
}
