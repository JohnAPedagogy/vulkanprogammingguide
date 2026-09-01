#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "=== Chapter 4.4 - Blit & Resolve ===\n";
    std::cout << "This lesson demonstrates image resolve and blit operations.\n\n";

    my_init_vulkan();
    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Failed to initialize Vulkan. Exiting.\n";
        return 1;
    }

    my_resolve_and_blit_image();

    std::cout << "\nCleaning up resources...\n";
    vk_cleanup();
    std::cout << "Completed successfully.\n";

    return 0;
}
