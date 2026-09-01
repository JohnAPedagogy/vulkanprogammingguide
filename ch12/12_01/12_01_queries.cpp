#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "Chapter 12.1 - Occlusion & Pipeline-Statistics Queries\n";
    std::cout << "=========================================================\n\n";

    my_init_vulkan();

    // Create an occlusion query pool
    std::cout << "\nCreating occlusion query pool...\n";
    my_create_query_pool(VK_QUERY_TYPE_OCCLUSION, 1);

    // Record, submit, and read back query results
    std::cout << "\nRecording and submitting query operations...\n";
    my_reset_and_query();

    std::cout << "\nCleaning up...\n";
    vk_cleanup();

    std::cout << "Shutdown complete.\n";
    return 0;
}
