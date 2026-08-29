#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

/* NOTES
 *
 * vk_<fname>() - (with underscore) are my vulkan setup functions separate from
 * vk<Function> - which are functions from the vulkan library
 */

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
static int device_count = 0;

VkResult vk_device_init_count(int *count)
{
    *count = 0;
    VkResult result = VK_SUCCESS;
    VkApplicationInfo appInfo = {};

    VkInstanceCreateInfo instanceCreateInfo = {};

    // Generic app info structure
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // create the m_instance
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        return VK_NOT_READY;
    }
    // First figure out how many devices are in the system
    uint32_t physicalDevCount = 0;
    result = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);
    if (result != VK_SUCCESS)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    // Size the device array appropriately
    // and get the physical device handles.
    // malloc allocation done here
    m_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDevCount);
    if (m_devices != nullptr)
    {
        vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, &m_devices[0]);
        *count = (int)physicalDevCount;
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }


    return result;
}

VkResult vk_cleanup()
{
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
    if (m_devices != nullptr)
    {
        free(m_devices);
        m_devices = nullptr;
    }
    return VK_SUCCESS;
}

VkResult vk_get_device_properties(int deviceIndex)
{
    if(device_count <= deviceIndex)
    { // invalid device index or no device ready
        return VK_NOT_READY;
    }
    uint32_t queueFamilyPropertyCount;
    VkQueueFamilyProperties* queueFamilyProperties; //aray of VkQueueFamilyPoperties requires cleanup
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
    // Get the memory properties of the physical device.
    vkGetPhysicalDeviceMemoryProperties(
        m_devices[deviceIndex],
        &physicalDeviceMemoryProperties);
    // First determine the number of queue families supported by the physical
    // device.
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[0],
        &queueFamilyPropertyCount,
        nullptr);
    // Allocate enough space for the queue property structures.
    queueFamilyProperties.resize(
        queueFamilyPropertyCount);
    // Now query the actual properties of the queue families.
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_physicalDevices[0],
        &queueFamilyPropertyCount,
        queueFamilyProperties.data());
}

int main()
{
    printf("Checking for physical graphics devices..\n");
    int device_count = 0;
    int rc = vk_device_init_count(&device_count);
    if(rc == VK_SUCCESS) {
        printf("Found %d physical graphics devices.\n", device_count);
    }else {
        switch(rc){
        case VK_ERROR_INITIALIZATION_FAILED:
            printf("Initialisation failed.\n");
            break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            printf("Out of host memory.\n");
            break;
        case VK_NOT_READY:
            printf("No instance found.\n");
            break;
        default:
            printf("Unkown error code %d", rc);
        }
    }
    vk_cleanup();
    return 0;
}
