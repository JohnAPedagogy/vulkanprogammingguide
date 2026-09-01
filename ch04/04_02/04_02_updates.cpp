#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "=== Chapter 4.2 - Updating Data at Runtime ===\n";
    std::cout << "This lesson demonstrates buffer updates via host-visible memory mapping.\n\n";

    my_init_vulkan();
    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Failed to initialize Vulkan. Exiting.\n";
        return 1;
    }

    my_update_buffer_and_readback();

    std::cout << "\nCleaning up resources...\n";
    vk_cleanup();
    std::cout << "Completed successfully.\n";

    return 0;
}
