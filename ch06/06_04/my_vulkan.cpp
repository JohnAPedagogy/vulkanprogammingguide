#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <iostream>
VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
VkDevice m_device = VK_NULL_HANDLE;
std::vector<vk_pipeline_cache> m_caches;
int device_count = 0;
static vk_allocator g_pipelineAllocator;
static const VkAllocationCallbacks g_pipelineAllocCallbacks = g_pipelineAllocator;
size_t count_enabled_features(const VkPhysicalDeviceFeatures *features)
{
    const VkBool32 *p = (const VkBool32 *)features;
    size_t count = 0;
    for (size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); ++i) if (p[i]) ++count;
    return count;
}
VkResult vk_device_init_count(int *count)
{
    *count = 0;
    VkApplicationInfo appInfo = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "06_04", .applicationVersion = VK_MAKE_VERSION(1, 0, 0), .pEngineName = "No Engine", .apiVersion = VK_API_VERSION_1_0};
    VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &appInfo};
#ifdef ENABLE_VALIDATION
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    ici.enabledLayerCount = 1;
    ici.ppEnabledLayerNames = validationLayers;
#endif
    VkResult vkr = vkCreateInstance(&ici, nullptr, &m_instance);
    if (vkr != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    uint32_t devCount = 0;
    vkr = vkEnumeratePhysicalDevices(m_instance, &devCount, nullptr);
    if (vkr != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    m_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * devCount);
    if (m_devices) { vkEnumeratePhysicalDevices(m_instance, &devCount, m_devices); *count = (int)devCount; }
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
    VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0, .queueCount = 1};
    float queuePriority = 1.0f;
    qci.pQueuePriorities = &queuePriority;
    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci, .pEnabledFeatures = &deviceFeatures};
    VkResult vkr = vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_device);
    if (vkr != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    if (feature_count) *feature_count = (int)count_enabled_features(&deviceFeatures);
    return vkr;
}
VkResult vk_create_pipeline_cache(vk_pipeline_cache *outCache)
{
    if (!outCache || m_device == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    VkPipelineCacheCreateInfo cci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, .initialDataSize = 0, .pInitialData = nullptr};
    VkResult vkr = vkCreatePipelineCache(m_device, &cci, &g_pipelineAllocCallbacks, &outCache->handle);
    return vkr == VK_SUCCESS ? vkr : VK_ERROR_INITIALIZATION_FAILED;
}
VkResult vk_track_pipeline_cache(vk_pipeline_cache *cache)
{
    if (!cache || cache->handle == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED;
    m_caches.push_back(*cache);
    return VK_SUCCESS;
}
VkResult vk_cleanup()
{
    for (const auto &c : m_caches) vkDestroyPipelineCache(m_device, c.handle, &g_pipelineAllocCallbacks);
    m_caches.clear();
    if (m_device != VK_NULL_HANDLE) { vkDestroyDevice(m_device, nullptr); m_device = VK_NULL_HANDLE; }
    if (m_instance != VK_NULL_HANDLE) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; }
    if (m_devices) { free(m_devices); m_devices = nullptr; }
    return VK_SUCCESS;
}
void my_init_vulkan(void) { int c = 0; VkResult vkr = vk_device_init_count(&c); device_count = (vkr == VK_SUCCESS) ? c : 0; if (vkr == VK_SUCCESS) std::cout << "Init OK: " << c << " devices\n"; }
void my_get_device_properties(int di) { uint32_t qc = 0; VkResult vkr = vk_get_device_properties(di, &qc); if (vkr == VK_SUCCESS) std::cout << "Device OK\n"; }
void my_get_logical_device(int di) { int fc = 0; VkResult vkr = vk_get_logical_device(di, &fc); if (vkr == VK_SUCCESS) std::cout << "Logical device OK\n"; }
void my_create_pipeline_cache(void) { vk_pipeline_cache cache; VkResult vkr = vk_create_pipeline_cache(&cache); if (vkr == VK_SUCCESS) { std::cout << "Pipeline cache created\n"; vk_track_pipeline_cache(&cache); } }
