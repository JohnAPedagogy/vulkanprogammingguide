#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "Chapter 12.3 - Reading Results Back to Host\n";
    std::cout << "==========================================\n\n";

    my_init_vulkan();

    // Create a source buffer (initialized with test data)
    VkDeviceSize bufferSize = 256; // bytes
    std::cout << "\nCreating source buffer (" << bufferSize << " bytes)...\n";
    my_create_source_buffer(bufferSize);

    // Create a staging buffer for readback
    std::cout << "\nCreating staging buffer for readback...\n";
    my_create_staging_buffer(bufferSize);

    // Copy from source to staging and read back
    std::cout << "\nCopying buffer and reading back results...\n";
    my_copy_and_readback(bufferSize);

    std::cout << "\nCleaning up...\n";
    vk_cleanup();

    std::cout << "Shutdown complete.\n";
    return 0;
}
