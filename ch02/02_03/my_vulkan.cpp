#include "my_vulkan.h"
#include "../0201_allocator.h"
#include <stddef.h>
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <string>


VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
VkDevice m_device = VK_NULL_HANDLE;
std::vector<vk_buffer> m_buffers;
int device_count = 0;

// Host allocation callbacks for buffer/memory objects, tracked via vk_allocator
// so buffer-related vkCreate*/vkAllocateMemory host allocations go through it
// instead of the driver's default allocator.
static vk_allocator g_bufferAllocator;
static const VkAllocationCallbacks g_bufferAllocCallbacks = g_bufferAllocator;


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
    VkResult result = VK_SUCCESS;
    VkApplicationInfo appInfo = {};

    VkInstanceCreateInfo instanceCreateInfo = {};

#ifdef ENABLE_VALIDATION
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
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

    if (*numInstanceExtensions == 0)
    {
        std::cout << "Warning: No instance extensions found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    instanceExtensionProperties.resize(*numInstanceExtensions);
    vkr = vkEnumerateInstanceExtensionProperties(nullptr,
                                                 numInstanceExtensions,
                                                 instanceExtensionProperties.data());
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumerateInstanceExtensionProperties failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
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
            memRequirements.size,
            memoryTypeIndex
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
    VkResult vkr = vk_get_device_properties(device_index, &dev_prop_count);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << dev_prop_count << " properties found for device[" << device_index << "]\n";
        break;
    case VK_NOT_READY:
        std::cout << "Warning: Graphics device not present.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Device queue family not found.\n";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        std::cout << "Warning: Out of host memory retrieving device properties.\n";
        break;
    default:
        std::cout << "Warning! vk_get_device_properties error=" << vkr << "\n";
    }
}


void my_get_logical_device(int device_index)
{
    int features = 0;
    VkResult vkr = vk_get_logical_device(device_index, &features);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << features << " features present on device[" << device_index << "]\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Invalid device index or no device available.\n";
        break;
    case VK_ERROR_FEATURE_NOT_PRESENT:
        std::cout << "Warning: Requested graphics feature(s) not supported.\n";
        break;
    default:
        std::cout << "Warning! vk_get_logical_device error=" << vkr << "\n";
    }
}

void my_get_layer_properties(int deviceIndex)
{
    uint32_t layer_count = 0;
    VkResult vkr = vk_get_layer_properties(deviceIndex, &layer_count);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << layer_count << " layers found!\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Invalid layer count pointer.\n";
        break;
    case VK_ERROR_LAYER_NOT_PRESENT:
        std::cout << "Warning: No instance or device layers present.\n";
        break;
    default:
        std::cout << "Warning! vk_get_layer_properties error=" << vkr << "\n";
    }
}

void my_get_extensions(void)
{
    uint32_t count = 0;
    VkResult vkr = vk_get_extensions(&count);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << count << " extensions found!\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not retrieve instance extensions.\n";
        break;
    default:
        std::cout << "Warning! vk_get_extensions error=" << vkr << "\n";
    }
}

void my_create_buffer(void)
{
    const VkDeviceSize size = 1024 * 1024;
    vk_buffer buf;
    VkResult vkr = vk_create_buffer(size,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     &buf);
    if (vkr == VK_SUCCESS)
    {
        vkr = vk_track_buffer(&buf, size,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Buffer created!\n";
        return;
    }

    switch (vkr)
    {
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Buffer object could not be created.\n";
        break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        std::cout << "Warning: Not enough device memory to back buffer.\n";
        break;
    default:
        std::cout << "Warning! vk_create_buffer error=" << vkr << "\n";
    }
}


// ---- dbg_* functions ----

void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    for(int i = 0; i < count; i++)
        std::cout << p[i].layerName << "\n";
}
