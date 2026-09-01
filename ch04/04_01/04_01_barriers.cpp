#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "=== Chapter 4.1 - Resource State & Barriers ===\n";
    std::cout << "This lesson demonstrates image memory barriers and layout transitions.\n\n";

    my_init_vulkan();
    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Failed to initialize Vulkan. Exiting.\n";
        return 1;
    }

    my_create_image_and_barrier();

    std::cout << "\nCleaning up resources...\n";
    vk_cleanup();
    std::cout << "Completed successfully.\n";

    return 0;
}
