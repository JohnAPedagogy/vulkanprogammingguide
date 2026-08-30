#include "my_vulkan.h"

int main()
{
    my_init_vulkan();
    if(device_count<=0)
    {
        printf("No graphics devices found!\n");
        return 0;
    }
    my_get_device_properties(0);
    my_get_logical_device(0);
    vk_cleanup();
    return 0;
}
