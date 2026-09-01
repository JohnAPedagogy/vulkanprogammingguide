#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <stddef.h>
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <string>
#include <fstream>
#include <vector>

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
VkDevice m_device = VK_NULL_HANDLE;
std::vector<vk_shader_module> m_shaderModules;
int device_count = 0;

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

static VkResult read_spirv_file_helper(const char *path, std::vector<uint32_t> *outCode)
{
    if (path == nullptr || outCode == nullptr)
    {
        std::cout << "read_spirv_file_helper: invalid parameters\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cout << "read_spirv_file_helper: cannot open file: " << path << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize % 4 != 0)
    {
        std::cout << "read_spirv_file_helper: file size not aligned to 4 bytes\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<char> buffer(fileSize);
    if (!file.read(buffer.data(), fileSize))
    {
        std::cout << "read_spirv_file_helper: failed to read file\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    outCode->resize(fileSize / 4);
    std::memcpy(outCode->data(), buffer.data(), fileSize);

    return VK_SUCCESS;
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
    appInfo.pApplicationName = "06_02 SPIR-V Binary";
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

VkResult vk_read_spirv_file(const char *path, std::vector<uint32_t> *outCode)
{
    VkResult vkr = VK_INCOMPLETE;

    vkr = read_spirv_file_helper(path, outCode);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "read_spirv_file_helper failed with result " << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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

    for (const vk_shader_module &mod : m_shaderModules)
    {
        vkDestroyShaderModule(m_device, mod.handle, &g_shaderAllocCallbacks);
    }
    m_shaderModules.clear();

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

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

void my_create_shader_module_from_file(const char *path)
{
    std::vector<uint32_t> spirvCode;
    VkResult vkr = vk_read_spirv_file(path, &spirvCode);

    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Could not read SPIR-V file.\n";
            break;
        default:
            std::cout << "Warning! vk_read_spirv_file unexpected error=" << vkr << "\n";
        }
        return;
    }

    vk_shader_module module;
    vkr = vk_create_shader_module(spirvCode.data(), spirvCode.size() * 4, &module);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Shader module created successfully from file.\n";
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


void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    if (p == nullptr || count <= 0)
        return;

    for (int i = 0; i < count; ++i)
    {
        std::cout << "Layer " << i << ": " << p[i].layerName << "\n";
    }
}
