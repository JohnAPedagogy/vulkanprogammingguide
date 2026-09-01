#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <stddef.h>
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <string>
#include <cstring>

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkDevice m_device = VK_NULL_HANDLE;
uint32_t m_queueFamilyIndex = UINT32_MAX;
std::vector<vk_buffer> m_buffers;

// Host allocation callbacks for buffer/memory objects
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

uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags properties)
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


// ---- vk_* setup functions ----

VkResult vk_init_instance(void)
{
    VkResult vkr = VK_INCOMPLETE;
    VkApplicationInfo appInfo = {};

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Ch04_02 Buffer Updates";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

#ifdef ENABLE_VALIDATION
    const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    instanceCreateInfo.enabledLayerCount = 1;
    instanceCreateInfo.ppEnabledLayerNames = validationLayers;
#endif

    vkr = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_get_physical_device(void)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_instance == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Instance not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t deviceCount = 0;
    vkr = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (vkr != VK_SUCCESS || deviceCount == 0)
    {
        std::cout << "vkEnumeratePhysicalDevices failed or found no devices, result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    VkPhysicalDevice devices[16];
    uint32_t queriedCount = std::min(deviceCount, (uint32_t)16);
    vkr = vkEnumeratePhysicalDevices(m_instance, &queriedCount, devices);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices(query) failed, result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    m_physicalDevice = devices[0];
    return VK_SUCCESS;
}

VkResult vk_get_queue_family(void)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Physical device not selected.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0)
    {
        std::cout << "Warning: No queue families found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkQueueFamilyProperties queueFamilies[16];
    uint32_t queriedCount = std::min(queueFamilyCount, (uint32_t)16);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queriedCount, queueFamilies);

    m_queueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queriedCount; ++i)
    {
        if (queueFamilies[i].queueCount > 0)
        {
            m_queueFamilyIndex = i;
            break;
        }
    }

    if (m_queueFamilyIndex == UINT32_MAX)
    {
        std::cout << "Warning: No queue family found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkResult vk_create_logical_device(void)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_physicalDevice == VK_NULL_HANDLE || m_queueFamilyIndex == UINT32_MAX)
    {
        std::cout << "Warning: Physical device or queue family not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
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
        std::cout << "vkCreateDevice failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    return VK_SUCCESS;
}


// ---- vk_* resource creation ----

VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create buffer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkBufferCreateInfo bufferCreateInfo = {
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
        const VkMemoryAllocateInfo allocInfo = {
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


// ---- vk_* buffer operations ----

VkResult vk_update_buffer(VkDeviceMemory memory, VkDeviceSize offset,
                          VkDeviceSize size, const void *data)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || memory == VK_NULL_HANDLE || data == nullptr)
    {
        std::cout << "Warning: Invalid device, memory, or data pointer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    void *mappedPtr = nullptr;
    vkr = vkMapMemory(m_device, memory, offset, size, 0, &mappedPtr);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkMapMemory failed with result=" << vkr << "\n";
        vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        return vkr;
    }

    std::memcpy(mappedPtr, data, size);
    vkUnmapMemory(m_device, memory);

    return VK_SUCCESS;
}

VkResult vk_readback_buffer(VkDeviceMemory memory, VkDeviceSize offset,
                            VkDeviceSize size, void *outData)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || memory == VK_NULL_HANDLE || outData == nullptr)
    {
        std::cout << "Warning: Invalid device, memory, or output pointer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    void *mappedPtr = nullptr;
    vkr = vkMapMemory(m_device, memory, offset, size, 0, &mappedPtr);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkMapMemory failed with result=" << vkr << "\n";
        vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        return vkr;
    }

    std::memcpy(outData, mappedPtr, size);
    vkUnmapMemory(m_device, memory);

    return VK_SUCCESS;
}


// ---- vk_* cleanup ----

VkResult vk_cleanup(void)
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

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    return VK_SUCCESS;
}


// ---- my_* orchestration functions ----

void my_init_vulkan(void)
{
#ifdef ENABLE_VALIDATION
    if (!getenv("VK_LAYER_PATH"))
    {
        _putenv_s("VK_LAYER_PATH", VK_LAYER_PATH);
        std::cout << "VK_LAYER_PATH = " << VK_LAYER_PATH << "\n";
    }
#endif

    VkResult vkr = vk_init_instance();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Instance created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create instance.\n";
        return;
    default:
        std::cout << "Warning! vk_init_instance error=" << vkr << "\n";
        return;
    }

    vkr = vk_get_physical_device();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Physical device selected.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get physical device.\n";
        vk_cleanup();
        return;
    default:
        std::cout << "Warning! vk_get_physical_device error=" << vkr << "\n";
        vk_cleanup();
        return;
    }

    vkr = vk_get_queue_family();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Queue family selected.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get queue family.\n";
        vk_cleanup();
        return;
    default:
        std::cout << "Warning! vk_get_queue_family error=" << vkr << "\n";
        vk_cleanup();
        return;
    }

    vkr = vk_create_logical_device();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Logical device created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create logical device.\n";
        vk_cleanup();
        return;
    default:
        std::cout << "Warning! vk_create_logical_device error=" << vkr << "\n";
        vk_cleanup();
        return;
    }
}

void my_update_buffer_and_readback(void)
{
    // Create a buffer with host-visible, host-coherent memory
    vk_buffer buf;
    VkResult vkr = vk_create_buffer(256,
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     &buf);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Buffer creation failed.\n";
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Buffer object could not be created.\n";
            break;
        default:
            std::cout << "vk_create_buffer error=" << vkr << "\n";
        }
        return;
    }

    vkr = vk_track_buffer(&buf, 256,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Buffer memory tracking failed.\n";
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Buffer memory allocation failed.\n";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cout << "Not enough device memory for buffer.\n";
            break;
        default:
            std::cout << "vk_track_buffer error=" << vkr << "\n";
        }
        return;
    }

    std::cout << "Buffer created: size=" << buf.size << " bytes\n";

    // Update the buffer with a test value
    const uint32_t testValue = 0xDEADBEEFu;
    vkr = vk_update_buffer(buf.memory, 0, sizeof(testValue), &testValue);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Buffer updated with value: 0x" << std::hex << testValue << std::dec << "\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Buffer update precondition failed.\n";
        return;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        std::cout << "Warning: Could not map buffer memory.\n";
        return;
    default:
        std::cout << "Warning! vk_update_buffer error=" << vkr << "\n";
        return;
    }

    // Read back the value
    uint32_t readbackValue = 0;
    vkr = vk_readback_buffer(buf.memory, 0, sizeof(readbackValue), &readbackValue);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Buffer readback value: 0x" << std::hex << readbackValue << std::dec << "\n";
        if (readbackValue == testValue)
        {
            std::cout << "SUCCESS: Readback matches written value!\n";
        }
        else
        {
            std::cout << "ERROR: Readback value mismatch! Expected 0x" << std::hex << testValue
                      << " but got 0x" << readbackValue << std::dec << "\n";
        }
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Readback precondition failed.\n";
        return;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        std::cout << "Warning: Could not map buffer memory for readback.\n";
        return;
    default:
        std::cout << "Warning! vk_readback_buffer error=" << vkr << "\n";
        return;
    }
}
