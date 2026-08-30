#include "my_vulkan.h"
#include <iostream>

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
    if (!supportedFeatures.tessellationShader || !supportedFeatures.geometryShader) {
        std::cout << "Device doesn't support required features!\n";
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
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
        std::cout << "Requested graphics feature(s) not supported.";
    }
    else
    {
        std::cout << features << " features pesent on device[" << device_index << "]\n";
    }
    vk_cleanup();
    return 0;
}
