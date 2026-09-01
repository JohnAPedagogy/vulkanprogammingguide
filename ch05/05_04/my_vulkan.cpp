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
VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
VkQueue m_presentQueue = VK_NULL_HANDLE;
std::vector<VkImage> m_swapchainImages;
std::vector<VkSemaphore> m_imageAvailableSemaphores;
std::vector<VkSemaphore> m_renderFinishedSemaphores;
int device_count = 0;
bool g_swapchainNeedsRebuild = false;

// Host allocation callbacks for window/surface/swapchain objects
static vk_allocator g_presentationAllocator;
static const VkAllocationCallbacks g_presentationAllocCallbacks = g_presentationAllocator;

// Host allocation callbacks for sync objects
static vk_allocator g_syncAllocator;
static const VkAllocationCallbacks g_syncAllocCallbacks = g_syncAllocator;

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

    // Create window without OpenGL context
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

    vkr = glfwCreateWindowSurface(m_instance, window, &g_presentationAllocCallbacks, outSurface);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "glfwCreateWindowSurface failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    return VK_SUCCESS;
}

VkResult vk_create_swapchain(uint32_t width, uint32_t height, VkSwapchainKHR *outSwapchain)
{
    VkResult vkr = VK_INCOMPLETE;

    if (outSwapchain == nullptr || width == 0 || height == 0)
    {
        std::cout << "Warning: Invalid swapchain parameters.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Guard clauses
    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Logical device not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_surface == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Surface not created yet.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Physical device not set.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities = {};
    vkr = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Query surface formats
    uint32_t formatCount = 0;
    vkr = vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetPhysicalDeviceSurfaceFormatsKHR (count) failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (formatCount == 0)
    {
        std::cout << "Warning: No surface formats available.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    if (formats == nullptr)
    {
        std::cout << "Warning: Failed to allocate formats array.\n";
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkr = vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetPhysicalDeviceSurfaceFormatsKHR (query) failed with result=" << vkr << "\n";
        free(formats);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    free(formats);

    // Query present modes
    uint32_t presentModeCount = 0;
    vkr = vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetPhysicalDeviceSurfacePresentModesKHR (count) failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (presentModeCount == 0)
    {
        std::cout << "Warning: No present modes available.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPresentModeKHR *presentModes = (VkPresentModeKHR *)malloc(presentModeCount * sizeof(VkPresentModeKHR));
    if (presentModes == nullptr)
    {
        std::cout << "Warning: Failed to allocate present modes array.\n";
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkr = vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetPhysicalDeviceSurfacePresentModesKHR (query) failed with result=" << vkr << "\n";
        free(presentModes);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    free(presentModes);

    // Determine image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    // Create swapchain
    const VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, // sType
        nullptr,                                      // pNext
        0,                                            // flags
        m_surface,                                    // surface
        imageCount,                                   // minImageCount
        surfaceFormat.format,                         // imageFormat
        surfaceFormat.colorSpace,                     // imageColorSpace
        {width, height},                              // imageExtent
        1,                                            // imageArrayLayers
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,         // imageUsage
        VK_SHARING_MODE_EXCLUSIVE,                    // imageSharingMode
        0,                                            // queueFamilyIndexCount
        nullptr,                                      // pQueueFamilyIndices
        capabilities.currentTransform,                // preTransform
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,           // compositeAlpha
        presentMode,                                  // presentMode
        VK_TRUE                                       // clipped
    };

    vkr = vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, &g_presentationAllocCallbacks, outSwapchain);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateSwapchainKHR failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkResult vk_get_swapchain_images(VkSwapchainKHR swapchain)
{
    VkResult vkr = VK_INCOMPLETE;

    if (swapchain == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid swapchain handle.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Logical device not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Query image count
    uint32_t imageCount = 0;
    vkr = vkGetSwapchainImagesKHR(m_device, swapchain, &imageCount, nullptr);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetSwapchainImagesKHR (count) failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (imageCount == 0)
    {
        std::cout << "Warning: No swapchain images returned.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Allocate space for images
    VkImage *images = (VkImage *)malloc(imageCount * sizeof(VkImage));
    if (images == nullptr)
    {
        std::cout << "Warning: Failed to allocate swapchain images array.\n";
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkr = vkGetSwapchainImagesKHR(m_device, swapchain, &imageCount, images);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetSwapchainImagesKHR (query) failed with result=" << vkr << "\n";
        free(images);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Store images in vector
    m_swapchainImages.clear();
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        m_swapchainImages.push_back(images[i]);
    }

    free(images);
    return VK_SUCCESS;
}

VkResult vk_create_semaphore(VkSemaphore *outSemaphore)
{
    VkResult vkr = VK_INCOMPLETE;

    if (outSemaphore == nullptr)
    {
        std::cout << "Warning: Null output semaphore pointer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Logical device not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkSemaphoreCreateInfo semaphoreCreateInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, // sType
        nullptr,                                  // pNext
        0                                         // flags
    };

    vkr = vkCreateSemaphore(m_device, &semaphoreCreateInfo, &g_syncAllocCallbacks, outSemaphore);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateSemaphore failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkResult vk_recreate_swapchain(uint32_t width, uint32_t height)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || m_surface == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Device or surface not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Wait for all in-flight frames to complete (§5.4 note)
    vkr = vkDeviceWaitIdle(m_device);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkDeviceWaitIdle failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Destroy old swapchain and images
    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, &g_presentationAllocCallbacks);
        m_swapchain = VK_NULL_HANDLE;
    }

    m_swapchainImages.clear();

    // Create new swapchain with new extent
    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    vkr = vk_create_swapchain(width, height, &newSwapchain);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to recreate swapchain.\n";
        return vkr;
    }

    m_swapchain = newSwapchain;

    // Get new swapchain images
    vkr = vk_get_swapchain_images(m_swapchain);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to get new swapchain images.\n";
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

    // Get GLFW required extensions
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Allocate array with GLFW extensions
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

    uint32_t totalExtensions = glfwExtensionCount;

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

VkResult vk_get_queue(uint32_t queueFamilyIndex, VkQueue *outQueue)
{
    VkResult vkr = VK_INCOMPLETE;

    if (outQueue == nullptr)
    {
        std::cout << "Warning: Null output queue pointer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Logical device not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetDeviceQueue(m_device, queueFamilyIndex, 0, outQueue);

    if (*outQueue == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Failed to get queue.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkResult vk_cleanup(void)
{
    VkResult vkr = VK_INCOMPLETE;

    // Destroy semaphores
    for (VkSemaphore sem : m_imageAvailableSemaphores)
    {
        if (sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_device, sem, &g_syncAllocCallbacks);
        }
    }
    m_imageAvailableSemaphores.clear();

    for (VkSemaphore sem : m_renderFinishedSemaphores)
    {
        if (sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_device, sem, &g_syncAllocCallbacks);
        }
    }
    m_renderFinishedSemaphores.clear();

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, &g_presentationAllocCallbacks);
        m_swapchain = VK_NULL_HANDLE;
    }

    m_swapchainImages.clear();

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, &g_presentationAllocCallbacks);
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
    VkResult vkr = vk_create_window(800, 600, "Vulkan - Resize & Fullscreen", &m_window);

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

void my_create_swapchain(uint32_t width, uint32_t height)
{
    VkResult vkr = vk_create_swapchain(width, height, &m_swapchain);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Swap chain created successfully with extent " << width << "x" << height << ".\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create swap chain.\n";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        std::cout << "Warning: Out of host memory creating swap chain.\n";
        break;
    default:
        std::cout << "Warning! vk_create_swapchain unexpected error=" << vkr << "\n";
    }
}

void my_get_swapchain_images(void)
{
    if (m_swapchain == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Swap chain not created.\n";
        return;
    }

    VkResult vkr = vk_get_swapchain_images(m_swapchain);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Swap chain images retrieved successfully. Image count: "
                  << m_swapchainImages.size() << "\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get swap chain images.\n";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        std::cout << "Warning: Out of host memory getting swap chain images.\n";
        break;
    default:
        std::cout << "Warning! vk_get_swapchain_images unexpected error=" << vkr << "\n";
    }
}

void my_create_frame_sync_objects(uint32_t frameCount)
{
    std::cout << "Creating " << frameCount << " frame synchronization objects...\n";

    for (uint32_t i = 0; i < frameCount; ++i)
    {
        VkSemaphore imageAvailableSem = VK_NULL_HANDLE;
        VkSemaphore renderFinishedSem = VK_NULL_HANDLE;

        VkResult vkr1 = vk_create_semaphore(&imageAvailableSem);
        VkResult vkr2 = vk_create_semaphore(&renderFinishedSem);

        if (vkr1 == VK_SUCCESS && vkr2 == VK_SUCCESS)
        {
            m_imageAvailableSemaphores.push_back(imageAvailableSem);
            m_renderFinishedSemaphores.push_back(renderFinishedSem);
        }
        else
        {
            std::cout << "Warning: Failed to create semaphores for frame " << i << "\n";
        }
    }

    std::cout << "Frame sync objects created: " << m_imageAvailableSemaphores.size() << " pairs.\n";
}

void my_get_present_queue(void)
{
    VkResult vkr = vk_get_queue(0, &m_presentQueue);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Present queue retrieved successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get present queue.\n";
        break;
    default:
        std::cout << "Warning! vk_get_queue unexpected error=" << vkr << "\n";
    }
}

void my_recreate_swapchain(uint32_t width, uint32_t height)
{
    std::cout << "Recreating swapchain with new extent " << width << "x" << height << "...\n";

    VkResult vkr = vk_recreate_swapchain(width, height);

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Swapchain recreated successfully.\n";
        g_swapchainNeedsRebuild = false;
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to recreate swapchain.\n";
        break;
    default:
        std::cout << "Warning! vk_recreate_swapchain unexpected error=" << vkr << "\n";
    }
}
