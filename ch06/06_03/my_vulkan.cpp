#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <iostream>
#include <string>
#include <cstring>

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
VkDevice m_device = VK_NULL_HANDLE;
std::vector<vk_shader_module> m_shaderModules;
int device_count = 0;

static vk_allocator g_shaderAllocator;
static const VkAllocationCallbacks g_shaderAllocCallbacks = g_shaderAllocator;

size_t count_enabled_features(const VkPhysicalDeviceFeatures *features)
{
    const VkBool32 *p = (const VkBool32 *)features;
    size_t count = 0;
    for (size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); ++i)
        if (p[i]) ++count;
    return count;
}

VkResult vk_device_init_count(int *count)
{
    *count = 0;
    VkApplicationInfo appInfo = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "06_03", .applicationVersion = VK_MAKE_VERSION(1, 0, 0), .pEngineName = "No Engine", .apiVersion = VK_API_VERSION_1_0};
    VkInstanceCreateInfo instanceCreateInfo = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &appInfo};
#ifdef ENABLE_VALIDATION
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
#endif
    VkResult vkr = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (vkr != VK_SUCCESS) { std::cout << "vkCreateInstance failed: " << vkr << "\n"; return VK_ERROR_INITIALIZATION_FAILED; }
    uint32_t physicalDevCount = 0;
    vkr = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);
    if (vkr != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    m_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDevCount);
    if (m_devices) { vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, m_devices); *count = (int)physicalDevCount; }
    return vkr;
}

VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount)
{
    if (!queueFamilyPropertyCount || deviceIndex < 0 || device_count <= deviceIndex || !m_devices) return VK_ERROR_INITIALIZATION_FAILED;
    vkGetPhysicalDeviceQueueFamilyProperties(m_devices[deviceIndex], queueFamilyPropertyCount, nullptr);
    return *queueFamilyPropertyCount > 0 ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

VkResult vk_get_logical_device(int device_index, int *feature_count)
{
    if (device_index < 0 || device_count <= device_index || !m_devices) return VK_ERROR_INITIALIZATION_FAILED;
    m_physicalDevice = m_devices[device_index];
    VkDeviceQueueCreateInfo queueCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0, .queueCount = 1};
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDeviceCreateInfo deviceCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &queueCreateInfo, .pEnabledFeatures = &deviceFeatures};
    VkResult vkr = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
    if (vkr != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (feature_count) *feature_count = (int)count_enabled_features(&deviceFeatures);
    return vkr;
}

VkResult vk_create_shader_module(const uint32_t *code, size_t codeSize, vk_shader_module *outModule)
{
    if (!code || !codeSize || !outModule || m_device == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    VkShaderModuleCreateInfo createInfo = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = codeSize, .pCode = code};
    VkResult vkr = vkCreateShaderModule(m_device, &createInfo, &g_shaderAllocCallbacks, &outModule->handle);
    return vkr == VK_SUCCESS ? vkr : VK_ERROR_INITIALIZATION_FAILED;
}

VkResult vk_track_shader_module(vk_shader_module *module)
{
    if (!module || module->handle == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    m_shaderModules.push_back(*module);
    return VK_SUCCESS;
}

VkResult vk_cleanup()
{
    for (const auto &mod : m_shaderModules) vkDestroyShaderModule(m_device, mod.handle, &g_shaderAllocCallbacks);
    m_shaderModules.clear();
    if (m_device != VK_NULL_HANDLE) { vkDestroyDevice(m_device, nullptr); m_device = VK_NULL_HANDLE; }
    if (m_instance != VK_NULL_HANDLE) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; }
    if (m_devices) { free(m_devices); m_devices = nullptr; }
    return VK_SUCCESS;
}

void my_init_vulkan(void)
{
    int count = 0;
    VkResult vkr = vk_device_init_count(&count);
    switch (vkr) {
    case VK_SUCCESS: std::cout << "Vulkan init OK, " << count << " devices\n"; device_count = count; break;
    default: std::cout << "Warning: Vulkan init failed\n";
    }
}

void my_get_device_properties(int deviceIndex)
{
    uint32_t queueFamilyPropertyCount = 0;
    VkResult vkr = vk_get_device_properties(deviceIndex, &queueFamilyPropertyCount);
    switch (vkr) {
    case VK_SUCCESS: std::cout << "Device OK, " << queueFamilyPropertyCount << " queue families\n"; break;
    default: std::cout << "Warning: Device properties failed\n";
    }
}

void my_get_logical_device(int deviceIndex)
{
    int featureCount = 0;
    VkResult vkr = vk_get_logical_device(deviceIndex, &featureCount);
    switch (vkr) {
    case VK_SUCCESS: std::cout << "Logical device OK\n"; break;
    default: std::cout << "Warning: Logical device creation failed\n";
    }
}

void my_create_shader_with_specialization(void)
{
    static const uint32_t trivialVertexSpirv[] = {0x07230203, 0x00010000, 0x00080007, 0x0000001d, 0x00000000, 0x00020011, 0x00000001, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001, 0x0006000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000d, 0x00030010, 0x00000004, 0x00000007, 0x00040047, 0x0000000d, 0x0000000b, 0x00000000, 0x00020013, 0x00000002, 0x00030021, 0x02000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x0000000d, 0x00000003, 0x00040015, 0x00000009, 0x00000020, 0x00000000, 0x0004002b, 0x00000009, 0x0000000a, 0x00000000, 0x00040020, 0x0000000b, 0x00000001, 0x00000007, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x0000000b, 0x0000000c, 0x00000003, 0x0000000a, 0x0004003d, 0x00000007, 0x0000000e, 0x0000000c, 0x0003003e, 0x0000000d, 0x0000000e, 0x000100fd, 0x00010038};
    vk_shader_module module;
    VkResult vkr = vk_create_shader_module(trivialVertexSpirv, sizeof(trivialVertexSpirv), &module);
    if (vkr == VK_SUCCESS) {
        std::cout << "Shader module created for specialization\n";
        VkSpecializationMapEntry specEntry = {.constantID = 0, .offset = 0, .size = sizeof(uint32_t)};
        uint32_t specValue = 1;
        VkSpecializationInfo specInfo = {.mapEntryCount = 1, .pMapEntries = &specEntry, .dataSize = sizeof(specValue), .pData = &specValue};
        std::cout << "Specialization info prepared (const_id=0, value=1)\n";
        vk_track_shader_module(&module);
    } else {
        std::cout << "Warning: Shader module creation failed\n";
    }
}

void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    if (!p || count <= 0) return;
    for (int i = 0; i < count; ++i) std::cout << "Layer " << i << ": " << p[i].layerName << "\n";
}
