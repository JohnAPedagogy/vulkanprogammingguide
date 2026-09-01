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
VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
VkQueue m_graphicsQueue = VK_NULL_HANDLE;
VkCommandPool m_commandPool = VK_NULL_HANDLE;
VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
std::vector<vk_image> m_images;
int device_count = 0;

// Hardcoded graphics queue family index. 02_03/02_04's vk_get_logical_device
// only ever requests queue family 0, so this project never picks a family -
// it assumes index 0 supports graphics, matching that existing convention.
static const uint32_t GRAPHICS_QUEUE_FAMILY_INDEX = 0;

// Host allocation callbacks for image/memory objects, tracked via vk_allocator
// so image-related vkCreate*/vkAllocateMemory host allocations go through it
// instead of the driver's default allocator.
static vk_allocator g_imageAllocator;
static const VkAllocationCallbacks g_imageAllocCallbacks = g_imageAllocator;

// Same idea, separate instance, for command-pool host allocations.
static vk_allocator g_cmdAllocator;
static const VkAllocationCallbacks g_cmdAllocCallbacks = g_cmdAllocator;


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

#ifdef ENABLE_VALIDATION
// Plain C function pointer callback - Vulkan delivers validation messages
// through this, not exceptions (the API has none). Must match
// PFN_vkDebugUtilsMessengerCallbackEXT exactly and return VK_FALSE (returning
// VK_TRUE would abort the call that triggered the message, which is a
// layer-authors-only debugging feature we don't want here).
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    (void)messageType;
    (void)pUserData;

    const char *severity = "INFO";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        severity = "ERROR";
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        severity = "WARNING";

    std::cout << "[validation " << severity << "] " << pCallbackData->pMessage << "\n";
    return VK_FALSE;
}
#endif

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
    VkResult result = VK_SUCCESS;
    VkApplicationInfo appInfo = {};

    VkInstanceCreateInfo instanceCreateInfo = {};

#ifdef ENABLE_VALIDATION
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;

    const char* instanceExtensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    instanceCreateInfo.enabledExtensionCount = 1;
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions;
#endif

    // Generic app info structure
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // create the m_instance
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    std::cout << "vkCreateInstance called\n";
    result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result" << result << "\n";
        return VK_NOT_READY;
    }
    std::cout << "vkCreateInstance succeeded\n";

    VkResult debugResult = vk_create_debug_messenger();
    if (debugResult != VK_SUCCESS)
        std::cout << "Warning: Debug messenger not active; validation output may not be visible.\n";

    // First figure out how many devices are in the system
    std::cout << "vkEnumeratePhysicalDevices called\n";
    uint32_t physicalDevCount = 0;
    result = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result " << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::cout << "vkEnumeratePhysicalDevices found " << physicalDevCount << " devices\n";

    // Size the device array appropriately
    // and get the physical device handles.
    // malloc allocation done here
    m_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDevCount);
    if (m_devices != nullptr)
    {
        std::cout << "malloc succeeded\n";
        vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, &m_devices[0]);
        *count = (int)physicalDevCount;
    }
    else
    {
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
        std::cout << "malloc failed\n";
    }


    return result;
}

VkResult vk_create_debug_messenger(void)
{
#ifndef ENABLE_VALIDATION
    return VK_SUCCESS;
#else
    if (m_instance == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No instance, cannot create debug messenger.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    auto createFn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (createFn == nullptr)
    {
        std::cout << "Warning: vkCreateDebugUtilsMessengerEXT not available (VK_EXT_debug_utils not enabled?).\n";
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    const VkDebugUtilsMessengerCreateInfoEXT createInfo =
    {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, nullptr,
        0,
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        debug_messenger_callback,
        nullptr
    };

    VkResult vkr = createFn(m_instance, &createInfo, nullptr, &m_debugMessenger);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDebugUtilsMessengerEXT failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
#endif
}

VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount)
{
    VkResult vkr = VK_INCOMPLETE;
    if(queueFamilyPropertyCount == nullptr || deviceIndex < 0 ||
        device_count <= deviceIndex || m_devices == nullptr)
    { // invalid device index or no device ready
        std::cout << "Warning Graphics device not present.\n";
        return VK_NOT_READY;
    }

    VkQueueFamilyProperties* queueFamilyProperties = nullptr; //array of VkQueueFamilyPoperties requires cleanup
    // First determine the number of queue families supported by the physical
    // device.
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);
    if(*queueFamilyPropertyCount == 0)
    {
        std::cout << "Warning Device family not found bailing...\n";
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


VkResult vk_cleanup()
{
    if (m_commandPool != VK_NULL_HANDLE)
    {
        // Freeing the pool implicitly frees every command buffer allocated
        // from it, including m_commandBuffer - no separate vkFreeCommandBuffers call needed.
        vkDestroyCommandPool(m_device, m_commandPool, &g_cmdAllocCallbacks);
        m_commandPool = VK_NULL_HANDLE;
        m_commandBuffer = VK_NULL_HANDLE;
    }
    for (const vk_image &img : m_images)
    {
        vkDestroyImage(m_device, img.handle, &g_imageAllocCallbacks);
        vkFreeMemory(m_device, img.memory, &g_imageAllocCallbacks);
    }
    m_images.clear();
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_devices != nullptr)
    {
        free(m_devices);
        m_devices = nullptr;
    }
#ifdef ENABLE_VALIDATION
    if (m_debugMessenger != VK_NULL_HANDLE)
    {
        auto destroyFn = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroyFn != nullptr)
            destroyFn(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }
#endif
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
    return VK_SUCCESS;
}


VkResult vk_get_logical_device(int device_index, int *feature_count)
{
    if(device_index < 0 || device_index >= device_count || m_devices == nullptr)
    {
        std::cout << "Warning: Invalid device index or no device available.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult result;
    VkPhysicalDeviceFeatures supportedFeatures;
    VkPhysicalDeviceFeatures requiredFeatures = {};

    // Get physical device features FIRST before requesting logical device
    vkGetPhysicalDeviceFeatures(m_devices[device_index],
                                &supportedFeatures);

    // Only request features that are supported by the device
    // This prevents vkCreateDevice from failing on GPUs without tessellation/geometry shaders
    requiredFeatures.multiDrawIndirect       =
        supportedFeatures.multiDrawIndirect;
    // Only request tessellation/geometry if supported
    requiredFeatures.tessellationShader      = supportedFeatures.tessellationShader;
    requiredFeatures.geometryShader          = supportedFeatures.geometryShader;

    const float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo =
        {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,     // sType
            nullptr,                                        // pNext
            0,                                               // flags
            0,                                               // queueFamilyIndex
            1,                                               // queueCount
            &queuePriority                                    // pQueuePriorities
        };
    *feature_count = (int)count_enabled_features(&supportedFeatures);
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

    if (m_devices[device_index] == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Physical device handle is null.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    result = vkCreateDevice(m_devices[device_index],
                            &deviceCreateInfo,
                            nullptr,
                            &m_device);

    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateDevice failed with result=" << result << "\n";
        // If features weren't supported, try without requiring tessellation/geometry
        if (result == VK_ERROR_FEATURE_NOT_PRESENT)
        {
            std::cout << "Attempting device creation without mandatory tessellation/geometry features...\n";
            requiredFeatures.tessellationShader      = VK_FALSE;
            requiredFeatures.geometryShader          = VK_FALSE;

            result = vkCreateDevice(m_devices[device_index],
                                    &deviceCreateInfo,
                                    nullptr,
                                    &m_device);
        }
        if (result != VK_SUCCESS)
        {
            return VK_ERROR_FEATURE_NOT_PRESENT;
        }
    }
    else
    {
        std::cout << "vkCreateDevice succeeded\n";
    }
    m_physicalDevice = m_devices[device_index];
    return VK_SUCCESS;
}


VkResult vk_get_layer_properties(int device_index, uint32_t *numInstanceLayers)
{
    VkResult vkr = VK_INCOMPLETE;
    VkLayerProperties* instanceLayerProperties = nullptr;
    VkLayerProperties* deviceLayerProperties = nullptr;

    if (numInstanceLayers == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;

    // Query the instance layers.
    vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                       nullptr);

    // If there are any layers, query their properties.
    if (*numInstanceLayers != 0)
    {
        instanceLayerProperties = (VkLayerProperties*)malloc(*numInstanceLayers * sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                           instanceLayerProperties);
        vkr = VK_SUCCESS;
    }
    else
    {
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    // DEVICE LAYER PROPERTIES

    // Query the device layers.
    uint32_t deviceLayerCnt;
    vkEnumerateDeviceLayerProperties(m_devices[device_index],
                                     &deviceLayerCnt,
                                       nullptr);

    // If there are any layers, query their properties.
    if (deviceLayerCnt != 0)
    {
        deviceLayerProperties = (VkLayerProperties*)malloc(deviceLayerCnt * sizeof(VkLayerProperties));
        vkEnumerateDeviceLayerProperties(m_devices[device_index],
                                         &deviceLayerCnt,
                                           deviceLayerProperties);
        vkr = VK_SUCCESS;
    }
    else
    {
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    std::cout << "Showing "<< *numInstanceLayers <<" Instance layer Properties***\n";
    dbg_show_layer_property_names(instanceLayerProperties, *numInstanceLayers);
    std::cout << "\nShowing "<< deviceLayerCnt  << " Device layer Properties***\n";
    dbg_show_layer_property_names(deviceLayerProperties, deviceLayerCnt);
    free(deviceLayerProperties);
    free(instanceLayerProperties);
    return vkr;
}


VkResult vk_get_extensions(uint32_t* numInstanceExtensions)
{
    VkResult vkr = VK_INCOMPLETE;
    std::vector<VkExtensionProperties> instanceExtensionProperties;

    // Query the instance extensions.
    vkEnumerateInstanceExtensionProperties(nullptr,
                                           numInstanceExtensions,
                                           nullptr);

    // If there are any extensions, query their properties.
    if (*numInstanceExtensions != 0)
    {
        instanceExtensionProperties.resize(*numInstanceExtensions);
        vkr = vkEnumerateInstanceExtensionProperties(nullptr,
                                                     numInstanceExtensions,
                                                     instanceExtensionProperties.data());
    }
    return vkr;
}


VkResult vk_create_image(VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, vk_image *outImage)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create image.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Format support must be checked before creation for the requested
    // tiling/usage combination - not every format supports every usage on
    // every device.
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &formatProperties);
    const VkFormatFeatureFlags requiredFeatures = VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((formatProperties.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
    {
        std::cout << "Warning: Requested format does not support transfer-dst optimal tiling.\n";
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    const VkImageCreateInfo imageCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr,
        0,
        VK_IMAGE_TYPE_2D,
        format,
        extent,
        1,                          // mipLevels
        1,                          // arrayLayers
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0, nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };

    vkr = vkCreateImage(m_device, &imageCreateInfo, &g_imageAllocCallbacks, &outImage->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImage failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    outImage->extent = extent;
    outImage->format = format;
    return vkr;
}

VkResult vk_track_image(vk_image *image, VkMemoryPropertyFlags properties)
{
    VkResult vkr = VK_INCOMPLETE;
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, image->handle, &memRequirements);

    uint32_t memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, properties);
    if (memoryTypeIndex == UINT32_MAX)
    {
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        goto destroy_image;
    }

    {
        const VkMemoryAllocateInfo allocInfo =
        {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
            memRequirements.size,
            memoryTypeIndex
        };

        vkr = vkAllocateMemory(m_device, &allocInfo, &g_imageAllocCallbacks, &image->memory);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkAllocateMemory failed with result=" << vkr << "\n";
            vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
            goto destroy_image;
        }
    }

    vkr = vkBindImageMemory(m_device, image->handle, image->memory, 0);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBindImageMemory failed with result=" << vkr << "\n";
        vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        goto free_memory;
    }

    m_images.push_back(*image);
    return vkr;

free_memory:
    vkFreeMemory(m_device, image->memory, &g_imageAllocCallbacks);
    image->memory = VK_NULL_HANDLE;
destroy_image:
    vkDestroyImage(m_device, image->handle, &g_imageAllocCallbacks);
    image->handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_get_device_queue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *outQueue)
{
    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot get queue.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // vkGetDeviceQueue has no return value - it cannot itself fail once the
    // device and family/index are valid, so this wrapper's only fallible
    // outcome is the precondition above.
    vkGetDeviceQueue(m_device, queueFamilyIndex, queueIndex, outQueue);
    return VK_SUCCESS;
}

VkResult vk_create_command_pool(uint32_t queueFamilyIndex, VkCommandPool *outPool)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create command pool.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkCommandPoolCreateInfo poolCreateInfo =
    {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
        0,
        queueFamilyIndex
    };

    vkr = vkCreateCommandPool(m_device, &poolCreateInfo, &g_cmdAllocCallbacks, outPool);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateCommandPool failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_allocate_command_buffer(VkCommandPool pool, VkCommandBuffer *outBuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (pool == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No command pool, cannot allocate command buffer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkCommandBufferAllocateInfo allocInfo =
    {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
        pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1
    };

    vkr = vkAllocateCommandBuffers(m_device, &allocInfo, outBuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkAllocateCommandBuffers failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_record_script(VkCommandBuffer commandBuffer, VkImage image)
{
    const VkCommandBufferBeginInfo begin =
    {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        0, nullptr
    };

    VkResult vkr = vkBeginCommandBuffer(commandBuffer, &begin);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBeginCommandBuffer failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // The book's listing assumes the image is "already created for
    // transfer-destination use"; vk_create_image above only creates it in
    // VK_IMAGE_LAYOUT_UNDEFINED, so a real runnable program needs an
    // explicit layout transition here before the clear is legal - this
    // barrier is not in the book listing, added so the recorded script
    // doesn't trip validation the moment it is submitted.
    const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    const VkImageMemoryBarrier toTransferDst =
    {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
        0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        image,
        range
    };
    vkCmdPipelineBarrier(commandBuffer,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0,
                          0, nullptr,
                          0, nullptr,
                          1, &toTransferDst);

    const VkClearColorValue color = {{0.1f, 0.2f, 0.3f, 1.0f}};
    vkCmdClearColorImage(commandBuffer, image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          &color, 1, &range);

    vkr = vkEndCommandBuffer(commandBuffer);
    std::cout << "recorded script: " << (vkr == VK_SUCCESS ? "yes" : "no") << "\n";
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEndCommandBuffer failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_submit_and_wait(VkQueue queue, VkCommandBuffer commandBuffer)
{
    if (queue == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No queue, cannot submit.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkSubmitInfo submit =
    {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr,
        0, nullptr, nullptr,
        1, &commandBuffer,
        0, nullptr
    };

    VkResult vkr = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkQueueSubmit failed with result=" << vkr << "\n";
        return VK_ERROR_DEVICE_LOST;
    }

    // No fence yet in this lesson (that's introduced in 3.4) - a full queue
    // wait is the simplest correct way to observe completion here.
    vkr = vkQueueWaitIdle(queue);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkQueueWaitIdle failed with result=" << vkr << "\n";
        return VK_ERROR_DEVICE_LOST;
    }
    return vkr;
}


// ---- my_* functions ----

void my_init_vulkan()
{
#ifdef ENABLE_VALIDATION
    // Point the Vulkan loader at the MSYS2 validation layer manifest so it
    // can find VK_LAYER_KHRONOS_validation without requiring VK_LAYER_PATH
    // to be set in the calling environment.
    if (!getenv("VK_LAYER_PATH"))
    {
        _putenv_s("VK_LAYER_PATH", VK_LAYER_PATH);
        std::cout << "VK_LAYER_PATH = " << VK_LAYER_PATH << "\n";

        std::cout << "--- vulkaninfo validation layer check ---\n";
#ifdef _WIN32
        std::string cmd = std::string(VK_LAYER_PATH) + "/vulkaninfo.exe --summary 2>&1";
#else
        std::string cmd = "vulkaninfo --summary 2>&1";
#endif
        FILE *pipe = popen(cmd.c_str(), "r");
        if (pipe)
        {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe))
            {
                std::string line(buf);
                // case-insensitive search for "valid" (covers "validation", "Valid", etc.)
                std::string lower = line;
                for (auto &c : lower) c = (char)tolower((unsigned char)c);
                if (lower.find("valid") != std::string::npos)
                    std::cout << line;
            }
            pclose(pipe);
        }
        std::cout << "--- end validation check ---\n";
    }
#endif

    std::cout << "Checking for physical graphics devices..\n";
    int rc = vk_device_init_count(&device_count);
    if(rc == VK_SUCCESS) {
        std::cout << "Found " << device_count << " physical graphics devices.\n";
    }else {
        switch(rc){
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Initialisation failed.\n";
            break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cout << "Out of host memory.\n";
            break;
        case VK_NOT_READY:
            std::cout << "No instance found.\n";
            break;
        default:
            std::cout << "Unkown error code " << rc << "\n";
        }
    }
    std::cout << "my_init_vulkan completed with  " << rc << "\n";
}

void my_get_device_properties(int device_index)
{
    uint32_t dev_prop_count = 0;
    int rc = vk_get_device_properties(device_index, &dev_prop_count);
    if(rc != VK_SUCCESS)
    {
        std::cout << "Failed to retrieve device properties\n";
    }
    else
    {
        std::cout << dev_prop_count <<" properties found for device[" << device_index << "]\n";
    }
}


void my_get_logical_device(int device_index)
{
    int features=0;
    VkResult vkr = vk_get_logical_device(device_index, &features);
    if(vkr != VK_SUCCESS)
    {
        std::cout << "Requested graphics feature(s) not supported.";
    }
    else
    {
        std::cout << features << " features present on device[" << device_index << "]\n";
    }
}

void my_get_layer_properties(int deviceIndex)
{
    uint32_t layer_count = 0;
    VkResult vkr = vk_get_layer_properties(deviceIndex, &layer_count);
    if(vkr == VK_SUCCESS)
    {
        std::cout << layer_count << " layers found!\n";
    }
    else
    {
        std::cout << "Erro: Layer not found!\n";
    }
}

void my_get_extensions(void)
{
    uint32_t count = 0;
    VkResult vkr = vk_get_extensions(&count);
    std::cout << count << " extensions found!\n";
    if(vkr != VK_SUCCESS)
        std::cout << "Warning! vk_get_extensions error.";
}

void my_create_image(void)
{
    const VkExtent3D extent = { 512, 512, 1 };
    vk_image img;
    VkResult vkr = vk_create_image(VK_FORMAT_R8G8B8A8_UNORM, extent,
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                    &img);
    if (vkr == VK_SUCCESS)
    {
        vkr = vk_track_image(&img, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Image created!\n";
        return;
    }

    switch (vkr)
    {
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        std::cout << "Warning: Requested image format/usage not supported by this device.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Image object could not be created.\n";
        break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        std::cout << "Warning: Not enough device memory to back image.\n";
        break;
    default:
        std::cout << "Warning! vk_create_image error=" << vkr << "\n";
    }
}

void my_get_device_queue(uint32_t queueFamilyIndex, uint32_t queueIndex)
{
    VkResult vkr = vk_get_device_queue(queueFamilyIndex, queueIndex, &m_graphicsQueue);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Graphics queue retrieved!\n";
        return;
    }
    std::cout << "Warning: Graphics queue could not be retrieved, error=" << vkr << "\n";
}

void my_create_command_pool(uint32_t queueFamilyIndex)
{
    VkResult vkr = vk_create_command_pool(queueFamilyIndex, &m_commandPool);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Command pool created!\n";
        return;
    }
    std::cout << "Warning: Command pool could not be created, error=" << vkr << "\n";
}

void my_allocate_command_buffer(void)
{
    VkResult vkr = vk_allocate_command_buffer(m_commandPool, &m_commandBuffer);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Command buffer allocated!\n";
        return;
    }
    std::cout << "Warning: Command buffer could not be allocated, error=" << vkr << "\n";
}

void my_record_script(void)
{
    if (m_images.empty())
    {
        std::cout << "Warning: No image tracked, cannot record script.\n";
        return;
    }

    VkResult vkr = vk_record_script(m_commandBuffer, m_images[0].handle);
    if (vkr != VK_SUCCESS)
        std::cout << "Warning: Script recording failed, error=" << vkr << "\n";
}

void my_submit_and_wait(void)
{
    VkResult vkr = vk_submit_and_wait(m_graphicsQueue, m_commandBuffer);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Script submitted and completed!\n";
        return;
    }
    std::cout << "Warning: Submission did not complete, error=" << vkr << "\n";
}


// ---- dbg_* functions ----

void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    for(int i = 0; i < count; i++)
        std::cout << p[i].layerName << "\n";
}
