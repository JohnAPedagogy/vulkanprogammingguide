#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "Chapter 12.2 - GPU Timing Queries\n";
    std::cout << "==================================\n\n";

    my_init_vulkan();

    // Create a timestamp query pool with 2 queries (start and end)
    std::cout << "\nCreating timestamp query pool...\n";
    my_create_timestamp_pool(2);

    // Record timestamps at different pipeline stages
    std::cout << "\nRecording timestamps...\n";
    my_record_timestamps();

    // Read back the results and calculate elapsed time
    std::cout << "\nReading timestamp results...\n";
    my_read_timestamp_results();

    std::cout << "\nCleaning up...\n";
    vk_cleanup();

    std::cout << "Shutdown complete.\n";
    return 0;
}
