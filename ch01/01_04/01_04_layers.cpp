#include "my_vulkan.h"

#include <iostream>

VkResult vk_get_layer_propeties(uint32_t *numInstanceLayers)
{
    VkResult vkr = VK_INCOMPLETE;
    VkLayerProperties* instanceLayerProperties = nullptr;

    if (numInstanceLayers == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;

    // Query the instance layers.
    vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                       nullptr);

    // If there are any layers, query their properties.
    if (*numInstanceLayers != 0)
    {
        instanceLayerProperties = (VkLayerProperties*)malloc(*numInstanceLayers * sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                           instanceLayerProperties);
        free(instanceLayerProperties);
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
    std::cout << "starting vulkan ...\n";
    my_init_vulkan();
    if(device_count<=0)
    {
        std::cout << "No graphics devices found!\n";
        return 0;
    }
    my_get_device_properties(0);
    my_get_logical_device(0);
    uint32_t layer_count = 0;
    VkResult vkr = vk_get_layer_propeties(&layer_count);
    if(vkr == VK_SUCCESS)
    {
        std::cout << layer_count << " layers found!\n";
    }
    else
    {
        std::cout << "Erro: Layer not found\n";
    }
    vk_cleanup();
    return 0;
}
