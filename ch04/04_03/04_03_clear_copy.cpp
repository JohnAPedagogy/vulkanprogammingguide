#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "=== Chapter 4.3 - Clearing & Copying Images ===\n";
    std::cout << "This lesson demonstrates image clearing and copying operations.\n\n";

    my_init_vulkan();
    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Failed to initialize Vulkan. Exiting.\n";
        return 1;
    }

    my_clear_and_copy_image();

    std::cout << "\nCleaning up resources...\n";
    vk_cleanup();
    std::cout << "Completed successfully.\n";

    return 0;
}
