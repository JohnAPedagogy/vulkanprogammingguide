#include "my_vulkan.h"

#include <iostream>


int main()
{
    std::cout << "starting vulkan ...\n";
    my_init_vulkan();
    if(device_count<=0)
    {
        std::cout << "No graphics devices found!\n";
        return 0;
    }
    my_get_device_properties(0);
    my_get_logical_device(0);
    my_get_layer_properties(0);
    my_get_extensions();
    my_create_buffer();
    my_create_image();
    vk_cleanup();
    return 0;
}
