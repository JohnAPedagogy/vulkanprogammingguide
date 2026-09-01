#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <iostream>
VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
VkDevice m_device = VK_NULL_HANDLE;
int device_count = 0;
static vk_allocator g_descriptorAllocator;
static const VkAllocationCallbacks g_descriptorAllocCallbacks = g_descriptorAllocator;
size_t count_enabled_features(const VkPhysicalDeviceFeatures *features) { const VkBool32 *p = (const VkBool32 *)features; size_t count = 0; for (size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); ++i) if (p[i]) ++count; return count; }
VkResult vk_device_init_count(int *count) { *count = 0; VkApplicationInfo appInfo = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "06_05", .applicationVersion = VK_MAKE_VERSION(1, 0, 0), .pEngineName = "No Engine", .apiVersion = VK_API_VERSION_1_0}; VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &appInfo}; #ifdef ENABLE_VALIDATION
    const char* vl[] = {"VK_LAYER_KHRONOS_validation"};
    ici.enabledLayerCount = 1;
    ici.ppEnabledLayerNames = vl;
#endif
    VkResult vkr = vkCreateInstance(&ici, nullptr, &m_instance);
    if (vkr != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    uint32_t dc = 0;
    vkr = vkEnumeratePhysicalDevices(m_instance, &dc, nullptr);
    if (vkr != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED;
    m_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * dc);
    if (m_devices) { vkEnumeratePhysicalDevices(m_instance, &dc, m_devices); *count = (int)dc; }
    return vkr;
}
VkResult vk_get_device_properties(int di, uint32_t *qfc) { if (!qfc || di < 0 || device_count <= di || !m_devices) return VK_ERROR_INITIALIZATION_FAILED; vkGetPhysicalDeviceQueueFamilyProperties(m_devices[di], qfc, nullptr); return *qfc > 0 ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED; }
VkResult vk_get_logical_device(int di, int *fc) { if (di < 0 || device_count <= di || !m_devices) return VK_ERROR_INITIALIZATION_FAILED; m_physicalDevice = m_devices[di]; VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0, .queueCount = 1}; float qp = 1.0f; qci.pQueuePriorities = &qp; VkPhysicalDeviceFeatures df = {}; VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci, .pEnabledFeatures = &df}; VkResult vkr = vkCreateDevice(m_physicalDevice, &dci, nullptr, &m_device); if (vkr != VK_SUCCESS) return VK_ERROR_INITIALIZATION_FAILED; if (fc) *fc = (int)count_enabled_features(&df); return vkr; }
VkResult vk_create_descriptor_set_layout(VkDescriptorSetLayout *outLayout) { if (!outLayout || m_device == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED; VkDescriptorSetLayoutBinding binding = {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT}; VkDescriptorSetLayoutCreateInfo dslci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding}; VkResult vkr = vkCreateDescriptorSetLayout(m_device, &dslci, &g_descriptorAllocCallbacks, outLayout); return vkr == VK_SUCCESS ? vkr : VK_ERROR_INITIALIZATION_FAILED; }
VkResult vk_update_descriptor_set(VkDescriptorSet set, uint32_t binding) { if (set == VK_NULL_HANDLE) return VK_ERROR_INITIALIZATION_FAILED; std::cout << "Descriptor set layout created for binding " << binding << "\n"; return VK_SUCCESS; }
VkResult vk_cleanup() { if (m_device != VK_NULL_HANDLE) { vkDestroyDevice(m_device, nullptr); m_device = VK_NULL_HANDLE; } if (m_instance != VK_NULL_HANDLE) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; } if (m_devices) { free(m_devices); m_devices = nullptr; } return VK_SUCCESS; }
void my_init_vulkan(void) { int c = 0; VkResult vkr = vk_device_init_count(&c); device_count = (vkr == VK_SUCCESS) ? c : 0; if (vkr == VK_SUCCESS) std::cout << "Init OK\n"; }
void my_get_device_properties(int di) { uint32_t qfc = 0; vk_get_device_properties(di, &qfc); if (qfc > 0) std::cout << "Device OK\n"; }
void my_get_logical_device(int di) { int fc = 0; VkResult vkr = vk_get_logical_device(di, &fc); if (vkr == VK_SUCCESS) std::cout << "Logical device OK\n"; }
void my_declare_resources(void) { VkDescriptorSetLayout layout; VkResult vkr = vk_create_descriptor_set_layout(&layout); if (vkr == VK_SUCCESS) { std::cout << "Descriptor set layout created\n"; vk_update_descriptor_set(VK_NULL_HANDLE, 0); vkDestroyDescriptorSetLayout(m_device, layout, &g_descriptorAllocCallbacks); } }
