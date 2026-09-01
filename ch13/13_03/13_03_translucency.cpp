#include "my_vulkan.h"

int main()
{
    std::cout << "Starting Vulkan translucency example (Lesson 13.3)...\n";

    my_init_vulkan();
    if(device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 0;
    }

    int active_device_index = 0;
    my_get_device_properties(active_device_index);
    my_get_logical_device(active_device_index);
    my_create_images();
    my_create_renderpass();
    my_create_framebuffer();
    my_create_shader_modules();
    my_create_pipelines();
    my_run_translucency_pass();

    std::cout << "Translucency example complete.\n";
    vk_cleanup();

    return 0;
}
