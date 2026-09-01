#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "=== Vulkan Tutorial 5.1: Surfaces ===\n\n";

    // Initialize Vulkan instance with GLFW extensions
    my_init_vulkan();

    if (device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 1;
    }

    // Create a GLFW window
    my_create_window();

    if (m_window == nullptr)
    {
        std::cout << "Failed to create window. Cleaning up.\n";
        vk_cleanup();
        return 1;
    }

    // Query device properties before creating logical device
    int active_device_index = 0;
    my_get_device_properties(active_device_index);

    // Create logical device
    my_get_logical_device(active_device_index);

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Failed to create logical device. Cleaning up.\n";
        vk_cleanup();
        return 1;
    }

    // Create a Vulkan surface from the GLFW window
    my_create_surface();

    if (m_surface == VK_NULL_HANDLE)
    {
        std::cout << "Failed to create surface. Cleaning up.\n";
        vk_cleanup();
        return 1;
    }

    // Check if the device supports presentation to this surface
    my_check_present_support(active_device_index);

    std::cout << "\nSurface created successfully and presentation support verified.\n";

    // Cleanup
    vk_cleanup();

    return 0;
}
