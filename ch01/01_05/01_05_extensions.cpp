#include "my_vulkan.h"

VkResult vk_get_logical_device(int device_index, int *feature_count)
{
    VkResult result;
    VkPhysicalDeviceFeatures supportedFeatures;
    VkPhysicalDeviceFeatures requiredFeatures = {};

    vkGetPhysicalDeviceFeatures(m_devices[device_index],
                                &supportedFeatures);

    requiredFeatures.multiDrawIndirect       =
        supportedFeatures.multiDrawIndirect;
    requiredFeatures.tessellationShader      = VK_TRUE;
    requiredFeatures.geometryShader          = VK_TRUE;

    const VkDeviceQueueCreateInfo deviceQueueCreateInfo =
        {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     // sType
            nullptr,                                        // pNext
            0,                                               // flags
            0,                                               // queueFamilyIndex
            1,                                               // queueCount
            nullptr                                          // pQueuePriorities
        };
    *feature_count = (int)count_enabled_features(&supportedFeatures);
    const VkDeviceCreateInfo deviceCreateInfo =
        {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,            // sType
            nullptr,                                         // pNext
            0,                                                // flags
            1,                                                // queueCreateInfoCount
            &deviceQueueCreateInfo,                           // pQueueCreateInfos
            0,                                                // enabledLayerCount
            nullptr,                                          // ppEnabledLayerNames
            0,                                                // enabledExtensionCount
            nullptr,                                          // ppEnabledExtensionNames
            &requiredFeatures                                 // pEnabledFeatures
        };

    result = vkCreateDevice(m_devices[device_index],
                            &deviceCreateInfo,
                            nullptr,
                            &m_device);

    if (result != VK_SUCCESS)
    {
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }else{
        return VK_SUCCESS;
    }
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
    int features=0;
    int device_index=0;
    VkResult vkr = vk_get_logical_device(device_index, &features);
    if(vkr != VK_SUCCESS)
    {
        printf("Requested graphics feature(s) not supported.");
    }
    else
    {
        printf("%d features pesent on device[%d]\n", features, device_index);
    }
    vk_cleanup();
    return 0;
}
