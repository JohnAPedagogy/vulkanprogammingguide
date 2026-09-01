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
int device_count = 0;

// Hardcoded graphics queue family index. 02_03/02_04's vk_get_logical_device
// only ever requests queue family 0, so this project never picks a family -
// it assumes index 0 supports graphics, matching that existing convention.
static const uint32_t GRAPHICS_QUEUE_FAMILY_INDEX = 0;

// Host allocation callbacks for command-pool host allocations.
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
        return VK_ERROR_INITIALIZATION_FAILED;
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
    {
        std::cout << "Warning Graphics device not present.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkQueueFamilyProperties* queueFamilyProperties = nullptr;
    // First determine the number of queue families supported by the physical device.
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


VkResult vk_create_command_pool(VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex, VkCommandPool *outPool)
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
        flags,
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

VkResult vk_allocate_command_buffers(VkCommandPool pool, uint32_t count, VkCommandBuffer *outBuffers)
{
    VkResult vkr = VK_INCOMPLETE;

    if (pool == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No command pool, cannot allocate command buffers.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (outBuffers == nullptr)
    {
        std::cout << "Warning: Invalid output buffer pointer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkCommandBufferAllocateInfo allocInfo =
    {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
        pool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        count
    };

    vkr = vkAllocateCommandBuffers(m_device, &allocInfo, outBuffers);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkAllocateCommandBuffers failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
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

void my_allocate_pool_batch(uint32_t queueFamilyIndex)
{
    const uint32_t requestedCount = 2;
    VkCommandPool pool = VK_NULL_HANDLE;

    // Create a resettable command pool
    VkResult vkr = vk_create_command_pool(
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        queueFamilyIndex,
        &pool);

    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Command pool could not be created.\n";
            break;
        default:
            std::cout << "Warning! vk_create_command_pool error=" << vkr << "\n";
        }
        return;
    }

    // Allocate a batch of command buffers from the pool
    VkCommandBuffer buffers[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    vkr = vk_allocate_command_buffers(pool, requestedCount, buffers);

    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Command buffers could not be allocated.\n";
            break;
        default:
            std::cout << "Warning! vk_allocate_command_buffers error=" << vkr << "\n";
        }
        vkDestroyCommandPool(m_device, pool, &g_cmdAllocCallbacks);
        return;
    }

    // Verify both buffers were allocated
    if (vkr == VK_SUCCESS && buffers[1] != VK_NULL_HANDLE)
    {
        std::cout << "allocated " << requestedCount << " command buffers: yes\n";
    }
    else
    {
        std::cout << "allocated " << requestedCount << " command buffers: no\n";
    }

    // Destroy the pool, which implicitly frees the allocated buffers
    vkDestroyCommandPool(m_device, pool, &g_cmdAllocCallbacks);
}


// ---- dbg_* functions ----

void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    for(int i = 0; i < count; i++)
        std::cout << p[i].layerName << "\n";
}
