#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "Starting 06_01: GLSL and Shader Loading\n";

    my_init_vulkan();
    if (device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 1;
    }

    int active_device_index = 0;
    my_get_device_properties(active_device_index);
    my_get_logical_device(active_device_index);
    my_create_shader_module();

    VkResult vkr = vk_cleanup();
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Vulkan cleanup successful.\n";
    }

    return 0;
}
