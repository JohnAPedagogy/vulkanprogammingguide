#include "my_vulkan.h"

VkResult vk_get_layer_propeties(uint32_t *numInstanceLayers)
{
    VkResult vkr = VK_INCOMPLETE;
    VkLayerProperties* instanceLayerProperties = nullptr;

    // Query the instance layers.
    vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                       nullptr);

    // If there are any layers, query their properties.
    if (numInstanceLayers != 0)
    {
        instanceLayerProperties = (VkLayerProperties*)malloc(*numInstanceLayers * sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                           instanceLayerProperties);
        vkr = VK_SUCCESS;
    }
    else
    {
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    return vkr;
}
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
