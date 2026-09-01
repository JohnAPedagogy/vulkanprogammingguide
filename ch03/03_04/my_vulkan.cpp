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
VkFence m_fence = VK_NULL_HANDLE;
std::vector<vk_buffer> m_buffers;
int device_count = 0;

// Hardcoded graphics queue family index.
static const uint32_t GRAPHICS_QUEUE_FAMILY_INDEX = 0;

// Host allocation callbacks for buffer/memory objects.
static vk_allocator g_bufferAllocator;
static const VkAllocationCallbacks g_bufferAllocCallbacks = g_bufferAllocator;

// Host allocation callbacks for command-pool host allocations.
static vk_allocator g_cmdAllocator;
static const VkAllocationCallbacks g_cmdAllocCallbacks = g_cmdAllocator;

// Host allocation callbacks for fence host allocations.
static vk_allocator g_fenceAllocator;
static const VkAllocationCallbacks g_fenceAllocCallbacks = g_fenceAllocator;


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

#ifdef ENABLE_VALIDATION
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

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    std::cout << "vkCreateInstance called\n";
    result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result" << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::cout << "vkCreateInstance succeeded\n";

    VkResult debugResult = vk_create_debug_messenger();
    if (debugResult != VK_SUCCESS)
        std::cout << "Warning: Debug messenger not active; validation output may not be visible.\n";

    std::cout << "vkEnumeratePhysicalDevices called\n";
    uint32_t physicalDevCount = 0;
    result = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result " << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::cout << "vkEnumeratePhysicalDevices found " << physicalDevCount << " devices\n";

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
        std::cout << "Warning: vkCreateDebugUtilsMessengerEXT not available.\n";
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
    {
        std::cout << "Warning Graphics device not present.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkQueueFamilyProperties* queueFamilyProperties = nullptr;
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);
    if(*queueFamilyPropertyCount == 0)
    {
        std::cout << "Warning Device family not found bailing...\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    queueFamilyProperties =(VkQueueFamilyProperties*)malloc(*queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
    if (queueFamilyProperties == nullptr)
    {
        vkr = VK_ERROR_OUT_OF_HOST_MEMORY;
        return vkr;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        queueFamilyProperties);
    vkr = VK_SUCCESS;
    free(queueFamilyProperties);
    return vkr;
}


VkResult vk_cleanup()
{
    if (m_fence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device, m_fence, &g_fenceAllocCallbacks);
        m_fence = VK_NULL_HANDLE;
    }
    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, &g_cmdAllocCallbacks);
        m_commandPool = VK_NULL_HANDLE;
        m_commandBuffer = VK_NULL_HANDLE;
    }
    for (const vk_buffer &buf : m_buffers)
    {
        vkDestroyBuffer(m_device, buf.handle, &g_bufferAllocCallbacks);
        vkFreeMemory(m_device, buf.memory, &g_bufferAllocCallbacks);
    }
    m_buffers.clear();
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

    vkGetPhysicalDeviceFeatures(m_devices[device_index],
                                &supportedFeatures);

    requiredFeatures.multiDrawIndirect       =
        supportedFeatures.multiDrawIndirect;
    requiredFeatures.tessellationShader      = supportedFeatures.tessellationShader;
    requiredFeatures.geometryShader          = supportedFeatures.geometryShader;

    const float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo =
        {
            VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            nullptr,
            0,
            0,
            1,
            &queuePriority
        };
    *feature_count = (int)count_enabled_features(&supportedFeatures);
    const VkDeviceCreateInfo deviceCreateInfo =
        {
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            nullptr,
            0,
            1,
            &deviceQueueCreateInfo,
            0,
            nullptr,
            0,
            nullptr,
            &requiredFeatures
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

    vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                       nullptr);

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

    uint32_t deviceLayerCnt;
    vkEnumerateDeviceLayerProperties(m_devices[device_index],
                                     &deviceLayerCnt,
                                       nullptr);

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

    vkEnumerateInstanceExtensionProperties(nullptr,
                                           numInstanceExtensions,
                                           nullptr);

    if (*numInstanceExtensions != 0)
    {
        instanceExtensionProperties.resize(*numInstanceExtensions);
        vkr = vkEnumerateInstanceExtensionProperties(nullptr,
                                                     numInstanceExtensions,
                                                     instanceExtensionProperties.data());
    }
    return vkr;
}


VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create buffer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkBufferCreateInfo bufferCreateInfo =
    {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr,
        0,
        size,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0, nullptr
    };

    vkr = vkCreateBuffer(m_device, &bufferCreateInfo, &g_bufferAllocCallbacks, &outBuffer->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateBuffer failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    return vkr;
}

VkResult vk_track_buffer(vk_buffer *buffer, VkDeviceSize size, VkMemoryPropertyFlags properties)
{
    VkResult vkr = VK_INCOMPLETE;
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer->handle, &memRequirements);

    uint32_t memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, properties);
    if (memoryTypeIndex == UINT32_MAX)
    {
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        goto destroy_buffer;
    }

    {
        const VkMemoryAllocateInfo allocInfo =
        {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr,
            memRequirements.size, memoryTypeIndex
        };

        vkr = vkAllocateMemory(m_device, &allocInfo, &g_bufferAllocCallbacks, &buffer->memory);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkAllocateMemory failed with result=" << vkr << "\n";
            vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
            goto destroy_buffer;
        }
    }

    vkr = vkBindBufferMemory(m_device, buffer->handle, buffer->memory, 0);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBindBufferMemory failed with result=" << vkr << "\n";
        vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        goto free_memory;
    }

    buffer->size = size;
    m_buffers.push_back(*buffer);
    return vkr;

free_memory:
    vkFreeMemory(m_device, buffer->memory, &g_bufferAllocCallbacks);
    buffer->memory = VK_NULL_HANDLE;
destroy_buffer:
    vkDestroyBuffer(m_device, buffer->handle, &g_bufferAllocCallbacks);
    buffer->handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_get_device_queue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *outQueue)
{
    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot get queue.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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

VkResult vk_create_fence(VkFence *outFence)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create fence.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkFenceCreateInfo fenceCreateInfo =
    {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr,
        0
    };

    vkr = vkCreateFence(m_device, &fenceCreateInfo, &g_fenceAllocCallbacks, outFence);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateFence failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_record_copy(VkCommandBuffer commandBuffer, VkBuffer source, VkBuffer destination, VkDeviceSize byteCount)
{
    VkResult vkr = VK_INCOMPLETE;

    if (commandBuffer == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No command buffer, cannot record copy.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkCommandBufferBeginInfo begin =
    {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
        0, nullptr
    };

    vkr = vkBeginCommandBuffer(commandBuffer, &begin);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBeginCommandBuffer failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    const VkBufferCopy region = { 0, 0, byteCount };
    vkCmdCopyBuffer(commandBuffer, source, destination, 1, &region);

    vkr = vkEndCommandBuffer(commandBuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEndCommandBuffer failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_submit_and_wait(VkQueue queue, VkCommandBuffer commandBuffer, VkFence fence)
{
    VkResult vkr = VK_INCOMPLETE;

    if (queue == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No queue, cannot submit.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (fence == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No fence, cannot wait.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkSubmitInfo submit =
    {
        VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr,
        0, nullptr, nullptr,
        1, &commandBuffer,
        0, nullptr
    };

    vkr = vkQueueSubmit(queue, 1, &submit, fence);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkQueueSubmit failed with result=" << vkr << "\n";
        vkr = VK_ERROR_DEVICE_LOST;
        return vkr;
    }

    vkr = vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkWaitForFences failed with result=" << vkr << "\n";
        vkr = VK_ERROR_DEVICE_LOST;
        return vkr;
    }

    vkr = vkResetFences(m_device, 1, &fence);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkResetFences failed with result=" << vkr << "\n";
        vkr = VK_ERROR_DEVICE_LOST;
    }
    return vkr;
}


// ---- my_* functions ----

void my_init_vulkan()
{
#ifdef ENABLE_VALIDATION
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
        default:
            std::cout << "Unknown error code " << rc << "\n";
        }
    }
    std::cout << "my_init_vulkan completed with " << rc << "\n";
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
        std::cout << "Error: Layer not found!\n";
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

void my_create_buffers(void)
{
    const VkDeviceSize bufferSize = 256;

    // Create source buffer
    vk_buffer srcBuf;
    VkResult vkr = vk_create_buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &srcBuf);
    if (vkr == VK_SUCCESS)
    {
        vkr = vk_track_buffer(&srcBuf, bufferSize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Source buffer could not be created.\n";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cout << "Warning: Not enough device memory to back source buffer.\n";
            break;
        default:
            std::cout << "Warning! vk_create_buffer error=" << vkr << "\n";
        }
        return;
    }

    // Create destination buffer
    vk_buffer dstBuf;
    vkr = vk_create_buffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &dstBuf);
    if (vkr == VK_SUCCESS)
    {
        vkr = vk_track_buffer(&dstBuf, bufferSize, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Destination buffer could not be created.\n";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cout << "Warning: Not enough device memory to back destination buffer.\n";
            break;
        default:
            std::cout << "Warning! vk_create_buffer error=" << vkr << "\n";
        }
        return;
    }

    std::cout << "Buffers created!\n";
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

void my_create_fence(void)
{
    VkResult vkr = vk_create_fence(&m_fence);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Fence created!\n";
        return;
    }
    std::cout << "Warning: Fence could not be created, error=" << vkr << "\n";
}

void my_record_copy(void)
{
    if (m_buffers.size() < 2)
    {
        std::cout << "Warning: Need source and destination buffers.\n";
        return;
    }

    VkBuffer source = m_buffers[0].handle;
    VkBuffer destination = m_buffers[1].handle;
    VkDeviceSize byteCount = m_buffers[0].size;

    VkResult vkr = vk_record_copy(m_commandBuffer, source, destination, byteCount);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Copy recorded!\n";
        return;
    }
    std::cout << "Warning: Copy recording failed, error=" << vkr << "\n";
}

void my_submit_and_wait(void)
{
    VkResult vkr = vk_submit_and_wait(m_graphicsQueue, m_commandBuffer, m_fence);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "submitted and completed: yes\n";
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
