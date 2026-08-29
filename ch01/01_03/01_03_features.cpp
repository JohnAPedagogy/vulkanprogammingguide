#include "my_vulkan.h"

VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount)
{
    VkResult vkr = VK_INCOMPLETE;
    if(queueFamilyPropertyCount == nullptr || deviceIndex < 0 ||
        device_count <= deviceIndex)
    { // invalid device index or no device ready
        printf("Warning Graphics device not present.");
        return VK_NOT_READY;
    }

    VkQueueFamilyProperties* queueFamilyProperties = nullptr; //aray of VkQueueFamilyPoperties requires cleanup
    // First determine the number of queue families supported by the physical
    // device.
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);
    if(*queueFamilyPropertyCount == 0)
    {
        printf("Warning Device family not found bailing...\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    // Allocate enough space for the queue property structures.
    queueFamilyProperties =(VkQueueFamilyProperties*)malloc(*queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
    if (queueFamilyProperties == nullptr)
    {
        vkr = VK_ERROR_OUT_OF_HOST_MEMORY;
        return vkr;
    }
    // Now query the actual properties of the queue families.
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        queueFamilyProperties);
    vkr = VK_SUCCESS;
    // cleanup queueFamilyProperties
    free(queueFamilyProperties);
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
    uint32_t dev_prop_count = 0;
    int device_index = 0;
    int rc = vk_get_device_properties(device_index, &dev_prop_count);
    if(rc != VK_SUCCESS)
    {
        printf("Failed to retrieve device properties\n");
    }
    else
    {
        printf("%d properties found for device[%d]\n",dev_prop_count, device_index );
    }
    vk_cleanup();
    return 0;
}
