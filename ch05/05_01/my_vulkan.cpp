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
GLFWwindow *m_window = nullptr;
VkSurfaceKHR m_surface = VK_NULL_HANDLE;
int device_count = 0;

// Host allocation callbacks for window/surface objects
static vk_allocator g_windowAllocator;
static const VkAllocationCallbacks g_windowAllocCallbacks = g_windowAllocator;

// Guard to ensure GLFW is initialized exactly once
static bool g_glfwInitialized = false;


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

VkResult vk_create_window(int width, int height, const char *title, GLFWwindow **outWindow)
{
    VkResult vkr = VK_INCOMPLETE;

    if (outWindow == nullptr || width <= 0 || height <= 0 || title == nullptr)
    {
        std::cout << "Warning: Invalid window parameters.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Initialize GLFW once
    if (!g_glfwInitialized)
    {
        if (!glfwInit())
        {
            const char *err;
            glfwGetError(&err);
            std::cout << "glfwInit failed: " << (err ? err : "unknown error") << "\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        g_glfwInitialized = true;
    }

    // Create window without OpenGL context (§12: GLFW_CLIENT_API, GLFW_NO_API)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (window == nullptr)
    {
        const char *err;
        glfwGetError(&err);
        std::cout << "glfwCreateWindow failed: " << (err ? err : "unknown error") << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *outWindow = window;
    vkr = VK_SUCCESS;
    return vkr;
}

VkResult vk_create_surface(GLFWwindow *window, VkSurfaceKHR *outSurface)
{
    VkResult vkr = VK_INCOMPLETE;

    if (window == nullptr || outSurface == nullptr)
    {
        std::cout << "Warning: Invalid window or output surface pointer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_instance == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Vulkan instance not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Thin wrapper over glfwCreateWindowSurface (§12)
    vkr = glfwCreateWindowSurface(m_instance, window, &g_windowAllocCallbacks, outSurface);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "glfwCreateWindowSurface failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    return VK_SUCCESS;
}

VkResult vk_device_init_count(int *count)
{
    VkResult vkr = VK_INCOMPLETE;

    if (count == nullptr)
    {
        std::cout << "Warning: Null count pointer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    *count = 0;

    VkApplicationInfo appInfo = {
        VK_STRUCTURE_TYPE_APPLICATION_INFO,  // sType
        nullptr,                               // pNext
        "Application",                         // pApplicationName
        VK_MAKE_VERSION(1, 0, 0),             // applicationVersion
        "No Engine",                           // pEngineName
        VK_MAKE_VERSION(1, 0, 0),             // engineVersion
        VK_API_VERSION_1_0                     // apiVersion
    };

    // Get GLFW required extensions (§12: glfwGetRequiredInstanceExtensions)
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Allocate array with GLFW extensions + VK_KHR_surface (may be included by GLFW)
    const char **extensions = (const char **)malloc((glfwExtensionCount + 1) * sizeof(const char *));
    if (extensions == nullptr)
    {
        std::cout << "Warning: Failed to allocate extensions array.\n";
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    // Copy GLFW extensions
    for (uint32_t i = 0; i < glfwExtensionCount; ++i)
    {
        extensions[i] = glfwExtensions[i];
    }

    // Ensure VK_KHR_surface is included (it's typically included by glfwGetRequiredInstanceExtensions on all platforms)
    uint32_t totalExtensions = glfwExtensionCount;
    bool hasSurface = false;
    for (uint32_t i = 0; i < glfwExtensionCount; ++i)
    {
        if (strcmp(glfwExtensions[i], VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        {
            hasSurface = true;
            break;
        }
    }

    VkInstanceCreateInfo instanceCreateInfo = {
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, // sType
        nullptr,                                  // pNext
        0,                                        // flags
        &appInfo,                                 // pApplicationInfo
        0,                                        // enabledLayerCount
        nullptr,                                  // ppEnabledLayerNames
        totalExtensions,                          // enabledExtensionCount
        extensions                                // ppEnabledExtensionNames
    };

#ifdef ENABLE_VALIDATION
    const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
#endif

    vkr = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    free(extensions);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    // Enumerate physical devices
    uint32_t physicalDevCount = 0;
    vkr = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    if (physicalDevCount == 0)
    {
        std::cout << "Warning: No physical devices found.\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    // Allocate and query devices
    m_devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * physicalDevCount);
    if (m_devices == nullptr)
    {
        std::cout << "Warning: Failed to allocate device array.\n";
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkr = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, m_devices);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices (second call) failed with result=" << vkr << "\n";
        free(m_devices);
        m_devices = nullptr;
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    *count = (int)physicalDevCount;
    return VK_SUCCESS;
}

VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount)
{
    VkResult vkr = VK_INCOMPLETE;

    if (queueFamilyPropertyCount == nullptr || deviceIndex < 0 ||
        device_count <= deviceIndex || m_devices == nullptr)
    {
        std::cout << "Warning: Invalid device index or no device available.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Query queue family properties count
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);

    if (*queueFamilyPropertyCount == 0)
    {
        std::cout << "Warning: No queue families found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkResult vk_get_logical_device(int device_index, int *feature_count)
{
    VkResult vkr = VK_INCOMPLETE;

    if (device_index < 0 || device_index >= device_count || m_devices == nullptr)
    {
        std::cout << "Warning: Invalid device index or no device available.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (feature_count == nullptr)
    {
        std::cout << "Warning: Null feature_count pointer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPhysicalDeviceFeatures supportedFeatures = {};
    VkPhysicalDeviceFeatures requiredFeatures = {};

    // Get supported features before requesting logical device
    vkGetPhysicalDeviceFeatures(m_devices[device_index], &supportedFeatures);

    // Copy only supported features to required
    requiredFeatures.multiDrawIndirect = supportedFeatures.multiDrawIndirect;
    requiredFeatures.tessellationShader = supportedFeatures.tessellationShader;
    requiredFeatures.geometryShader = supportedFeatures.geometryShader;

    const float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, // sType
        nullptr,                                     // pNext
        0,                                           // flags
        0,                                           // queueFamilyIndex
        1,                                           // queueCount
        &queuePriority                               // pQueuePriorities
    };

    *feature_count = (int)count_enabled_features(&supportedFeatures);

    const VkDeviceCreateInfo deviceCreateInfo = {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,  // sType
        nullptr,                                // pNext
        0,                                      // flags
        1,                                      // queueCreateInfoCount
        &deviceQueueCreateInfo,                 // pQueueCreateInfos
        0,                                      // enabledLayerCount
        nullptr,                                // ppEnabledLayerNames
        0,                                      // enabledExtensionCount
        nullptr,                                // ppEnabledExtensionNames
        &requiredFeatures                       // pEnabledFeatures
    };

    vkr = vkCreateDevice(m_devices[device_index], &deviceCreateInfo, nullptr, &m_device);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDevice failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    m_physicalDevice = m_devices[device_index];
    return VK_SUCCESS;
}

VkResult vk_get_present_support(int deviceIndex, VkBool32 *outSupported)
{
    VkResult vkr = VK_INCOMPLETE;

    if (outSupported == nullptr || deviceIndex < 0 ||
        device_count <= deviceIndex || m_devices == nullptr)
    {
        std::cout << "Warning: Invalid parameters for present support check.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_surface == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Surface not created yet.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Check if queue family 0 supports presentation to this surface
    vkr = vkGetPhysicalDeviceSurfaceSupportKHR(m_devices[deviceIndex], 0, m_surface, outSupported);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetPhysicalDeviceSurfaceSupportKHR failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    return VK_SUCCESS;
}

VkResult vk_cleanup(void)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, &g_windowAllocCallbacks);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_window != nullptr)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    if (g_glfwInitialized)
    {
        glfwTerminate();
        g_glfwInitialized = false;
    }

    if (m_devices != nullptr)
    {
        free(m_devices);
        m_devices = nullptr;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    return VK_SUCCESS;
}


// ---- my_* functions ----

void my_init_vulkan(void)
{
    VkResult vkr = vk_device_init_count(&device_count);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Vulkan instance created successfully with " << device_count << " devices.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to initialize Vulkan instance.\n";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        std::cout << "Warning: Out of host memory during Vulkan initialization.\n";
        break;
    default:
        std::cout << "Warning! vk_device_init_count unexpected error=" << vkr << "\n";
    }
}

void my_create_window(void)
{
    VkResult vkr = vk_create_window(800, 600, "Vulkan - Surface Creation", &m_window);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "GLFW window created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create GLFW window.\n";
        break;
    default:
        std::cout << "Warning! vk_create_window unexpected error=" << vkr << "\n";
    }
}

void my_create_surface(void)
{
    VkResult vkr = vk_create_surface(m_window, &m_surface);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Vulkan surface created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create Vulkan surface.\n";
        break;
    default:
        std::cout << "Warning! vk_create_surface unexpected error=" << vkr << "\n";
    }
}

void my_get_device_properties(int deviceIndex)
{
    uint32_t queueFamilyPropertyCount = 0;
    VkResult vkr = vk_get_device_properties(deviceIndex, &queueFamilyPropertyCount);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Device properties queried successfully. Queue families: "
                  << queueFamilyPropertyCount << "\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get device properties.\n";
        break;
    default:
        std::cout << "Warning! vk_get_device_properties unexpected error=" << vkr << "\n";
    }
}

void my_get_logical_device(int deviceIndex)
{
    int feature_count = 0;
    VkResult vkr = vk_get_logical_device(deviceIndex, &feature_count);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Logical device created successfully with " << feature_count
                  << " enabled features.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create logical device.\n";
        break;
    default:
        std::cout << "Warning! vk_get_logical_device unexpected error=" << vkr << "\n";
    }
}

void my_check_present_support(int deviceIndex)
{
    VkBool32 supported = VK_FALSE;
    VkResult vkr = vk_get_present_support(deviceIndex, &supported);

    switch (vkr)
    {
    case VK_SUCCESS:
        if (supported)
        {
            std::cout << "Device supports presentation to this surface.\n";
        }
        else
        {
            std::cout << "Warning: Device does not support presentation to this surface.\n";
        }
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to check presentation support.\n";
        break;
    default:
        std::cout << "Warning! vk_get_present_support unexpected error=" << vkr << "\n";
    }
}
