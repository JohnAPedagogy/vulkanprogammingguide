#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <stddef.h>
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <string>

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
VkDevice m_device = VK_NULL_HANDLE;
std::vector<vk_shader_module> m_shaderModules;
int device_count = 0;

// Host allocation callbacks for shader objects
static vk_allocator g_shaderAllocator;
static const VkAllocationCallbacks g_shaderAllocCallbacks = g_shaderAllocator;


// ---- helper functions ----

size_t count_enabled_features(const VkPhysicalDeviceFeatures *features)
{
    const VkBool32 *p = (const VkBool32 *)features;
    size_t count = 0;

    for (size_t i = 0;
         i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
         ++i)
    {
        if (p[i])
            ++count;
    }

    return count;
}

static uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    std::cout << "Warning: No suitable memory type found.\n";
    return UINT32_MAX;
}


// ---- vk_* functions ----

VkResult vk_device_init_count(int *count)
{
    *count = 0;
    VkResult vkr = VK_INCOMPLETE;
    VkApplicationInfo appInfo = {};

    VkInstanceCreateInfo instanceCreateInfo = {};

#ifdef ENABLE_VALIDATION
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
#endif

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "06_01 Shader Loading";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    vkr = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result " << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t physicalDevCount = 0;
    vkr = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result " << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDevCount);
    if (m_devices != nullptr)
    {
        vkr = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, &m_devices[0]);
        if (vkr == VK_SUCCESS)
        {
            *count = (int)physicalDevCount;
        }
    }
    else
    {
        std::cout << "malloc failed for device list\n";
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    return vkr;
}

VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount)
{
    VkResult vkr = VK_INCOMPLETE;
    if (queueFamilyPropertyCount == nullptr || deviceIndex < 0 ||
        device_count <= deviceIndex || m_devices == nullptr)
    {
        std::cout << "Warning: Graphics device not present.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);

    if (*queueFamilyPropertyCount == 0)
    {
        std::cout << "Warning: Device family not found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkr = VK_SUCCESS;
    return vkr;
}

VkResult vk_get_logical_device(int device_index, int *feature_count)
{
    VkResult vkr = VK_INCOMPLETE;

    if (device_index < 0 || device_count <= device_index || m_devices == nullptr)
    {
        std::cout << "Warning: No device available.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_physicalDevice = m_devices[device_index];

    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = 0;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    vkr = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDevice failed with result " << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (feature_count)
        *feature_count = (int)count_enabled_features(&deviceFeatures);

    return vkr;
}

VkResult vk_create_shader_module(const uint32_t *code, size_t codeSize, vk_shader_module *outModule)
{
    VkResult vkr = VK_INCOMPLETE;

    if (code == nullptr || codeSize == 0 || outModule == nullptr)
    {
        std::cout << "Warning: Invalid parameters to vk_create_shader_module.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create shader module.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = codeSize,
        .pCode = code
    };

    vkr = vkCreateShaderModule(m_device, &createInfo, &g_shaderAllocCallbacks, &outModule->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateShaderModule failed with result " << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_track_shader_module(vk_shader_module *module)
{
    VkResult vkr = VK_INCOMPLETE;

    if (module == nullptr || module->handle == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid shader module to track.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_shaderModules.push_back(*module);
    vkr = VK_SUCCESS;
    return vkr;
}

VkResult vk_cleanup()
{
    VkResult vkr = VK_SUCCESS;

    // Destroy shader modules
    for (const vk_shader_module &mod : m_shaderModules)
    {
        vkDestroyShaderModule(m_device, mod.handle, &g_shaderAllocCallbacks);
    }
    m_shaderModules.clear();

    // Destroy device
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    // Destroy instance
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    // Free device array
    if (m_devices != nullptr)
    {
        free(m_devices);
        m_devices = nullptr;
    }

    return vkr;
}


// ---- my_* functions ----

void my_init_vulkan(void)
{
    int count = 0;
    VkResult vkr = vk_device_init_count(&count);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Vulkan instance created successfully. Device count: " << count << "\n";
        device_count = count;
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Vulkan initialization failed.\n";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        std::cout << "Warning: Out of host memory during Vulkan initialization.\n";
        break;
    default:
        std::cout << "Warning! vk_device_init_count unexpected error=" << vkr << "\n";
    }
}

void my_get_device_properties(int deviceIndex)
{
    uint32_t queueFamilyPropertyCount = 0;
    VkResult vkr = vk_get_device_properties(deviceIndex, &queueFamilyPropertyCount);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Device properties retrieved. Queue families: " << queueFamilyPropertyCount << "\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not retrieve device properties.\n";
        break;
    default:
        std::cout << "Warning! vk_get_device_properties unexpected error=" << vkr << "\n";
    }
}

void my_get_logical_device(int deviceIndex)
{
    int featureCount = 0;
    VkResult vkr = vk_get_logical_device(deviceIndex, &featureCount);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Logical device created successfully. Enabled features: " << featureCount << "\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create logical device.\n";
        break;
    default:
        std::cout << "Warning! vk_get_logical_device unexpected error=" << vkr << "\n";
    }
}

void my_create_shader_module(void)
{
    // Embedded SPIR-V bytes for trivial shader
    // This is a simple pass-through vertex shader compiled to SPIR-V
    // glslc trivial.vert -o trivial.vert.spv produces these bytes
    static const uint32_t trivialVertexSpirv[] = {
        0x07230203, 0x00010000, 0x00080007, 0x0000001d, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
        0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
        0x0006000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000d, 0x00030010, 0x00000004,
        0x00000007, 0x00040047, 0x0000000d, 0x0000000b, 0x00000000, 0x00020013, 0x00000002, 0x00030021,
        0x02000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006,
        0x00000004, 0x00040020, 0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x0000000d,
        0x00000003, 0x00040015, 0x00000009, 0x00000020, 0x00000000, 0x0004002b, 0x00000009, 0x0000000a,
        0x00000000, 0x00040020, 0x0000000b, 0x00000001, 0x00000007, 0x00050036, 0x00000002, 0x00000004,
        0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x0000000b, 0x0000000c, 0x00000003,
        0x0000000a, 0x0004003d, 0x00000007, 0x0000000e, 0x0000000c, 0x0003003e, 0x0000000d, 0x0000000e,
        0x000100fd, 0x00010038
    };

    vk_shader_module module;
    VkResult vkr = vk_create_shader_module(trivialVertexSpirv, sizeof(trivialVertexSpirv), &module);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Shader module created successfully.\n";
        vkr = vk_track_shader_module(&module);
        if (vkr == VK_SUCCESS)
        {
            std::cout << "Shader module tracked successfully.\n";
        }
        else
        {
            std::cout << "Warning: Failed to track shader module.\n";
        }
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Shader module could not be created.\n";
        break;
    default:
        std::cout << "Warning! vk_create_shader_module unexpected error=" << vkr << "\n";
    }
}


// ---- dbg_* functions ----

void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    if (p == nullptr || count <= 0)
        return;

    for (int i = 0; i < count; ++i)
    {
        std::cout << "Layer " << i << ": " << p[i].layerName << "\n";
    }
}
