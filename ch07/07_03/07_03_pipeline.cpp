#include "my_vulkan.h"
#include <iostream>

int main()
{
    my_init_vulkan();
    if(device_count<=0) return 0;
    int active_device_index = 0;
    my_get_device_properties(active_device_index);
    my_get_logical_device(active_device_index);
    my_get_layer_properties(active_device_index);
    my_get_extensions();
    my_create_graphics_pipeline();
    vk_cleanup();
    return 0;
}
