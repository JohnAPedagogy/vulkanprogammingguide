#include "my_vulkan.h"

#include <iostream>

int main()
{
    std::cout << "Starting Vulkan event test...\n";
    my_init_vulkan();
    if (device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 0;
    }

    int features = 0;
    VkResult vkr = vk_get_device_properties(0, nullptr);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to get device properties.\n";
        vk_cleanup();
        return -1;
    }

    vkr = vk_get_logical_device(0, &features);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create logical device.\n";
        vk_cleanup();
        return -1;
    }

    std::cout << features << " features available on device[0]\n";

    my_run_event_test();

    vk_cleanup();
    std::cout << "Event test completed and cleaned up.\n";
    return 0;
}
