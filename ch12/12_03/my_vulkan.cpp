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
VkQueue m_graphicsQueue = VK_NULL_HANDLE;
VkCommandPool m_commandPool = VK_NULL_HANDLE;
VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
std::vector<vk_buffer> m_buffers;

// Host allocation callbacks for buffer objects
static vk_allocator g_bufferAllocator;
static const VkAllocationCallbacks g_bufferAllocCallbacks = g_bufferAllocator;

// Store source and staging buffer indices for readback operations
static int g_sourceBufferIndex = -1;
static int g_stagingBufferIndex = -1;

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

VkResult vk_init_instance()
{
    VkResult vkr = VK_INCOMPLETE;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Chapter 12.3 - Buffer Readback";
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

    vkr = vkCreateInstance(&instanceCreateInfo, &g_bufferAllocCallbacks, &m_instance);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_init_device()
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
        std::cout << "vkEnumeratePhysicalDevices failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkr = vkEnumeratePhysicalDevices(m_instance, &deviceCount, &m_physicalDevice);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices(2) failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

    VkQueueFamilyProperties *queueFamilies =
        (VkQueueFamilyProperties*)malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    if (!queueFamilies)
    {
        std::cout << "malloc failed for queue families\n";
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies);

    uint32_t graphicsQueueFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphicsQueueFamily = i;
            break;
        }
    }

    free(queueFamilies);

    if (graphicsQueueFamily == UINT32_MAX)
    {
        std::cout << "Warning: No graphics queue family found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    vkr = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, &g_bufferAllocCallbacks, &m_device);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDevice failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_init_queues()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Device not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetDeviceQueue(m_device, 0, 0, &m_graphicsQueue);
    if (m_graphicsQueue == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Failed to get graphics queue.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkr = VK_SUCCESS;
    return vkr;
}

VkResult vk_init_command_pool()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Device not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = 0;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    vkr = vkCreateCommandPool(m_device, &poolInfo, &g_bufferAllocCallbacks, &m_commandPool);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateCommandPool failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_allocate_command_buffer(VkCommandBuffer *outCmd)
{
    VkResult vkr = VK_INCOMPLETE;

    if (outCmd == nullptr)
    {
        std::cout << "Warning: outCmd pointer is null.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_commandPool == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Command pool not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vkr = vkAllocateCommandBuffers(m_device, &allocInfo, outCmd);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkAllocateCommandBuffers failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (outBuffer == nullptr)
    {
        std::cout << "Warning: outBuffer pointer is null.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Device not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (size == 0)
    {
        std::cout << "Warning: Buffer size is zero.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkr = vkCreateBuffer(m_device, &bufferInfo, &g_bufferAllocCallbacks, &outBuffer->handle);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkCreateBuffer failed with result=" << vkr << "\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }
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

VkResult vk_cleanup()
{
    for (const vk_buffer &buf : m_buffers)
    {
        if (buf.handle != VK_NULL_HANDLE)
            vkDestroyBuffer(m_device, buf.handle, &g_bufferAllocCallbacks);
        if (buf.memory != VK_NULL_HANDLE)
            vkFreeMemory(m_device, buf.memory, &g_bufferAllocCallbacks);
    }
    m_buffers.clear();

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, &g_bufferAllocCallbacks);
        m_commandPool = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, &g_bufferAllocCallbacks);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, &g_bufferAllocCallbacks);
        m_instance = VK_NULL_HANDLE;
    }

    return VK_SUCCESS;
}

// ---- my_* functions ----

void my_init_vulkan(void)
{
    VkResult vkr = vk_init_instance();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Instance created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Instance initialization failed.\n";
        return;
    default:
        std::cout << "Warning! vk_init_instance error=" << vkr << "\n";
        return;
    }

    vkr = vk_init_device();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Device created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Device initialization failed.\n";
        return;
    default:
        std::cout << "Warning! vk_init_device error=" << vkr << "\n";
        return;
    }

    vkr = vk_init_queues();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Queues retrieved successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Queue initialization failed.\n";
        return;
    default:
        std::cout << "Warning! vk_init_queues error=" << vkr << "\n";
        return;
    }

    vkr = vk_init_command_pool();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Command pool created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Command pool initialization failed.\n";
        return;
    default:
        std::cout << "Warning! vk_init_command_pool error=" << vkr << "\n";
        return;
    }

    vkr = vk_allocate_command_buffer(&m_commandBuffer);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Command buffer allocated successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Command buffer allocation failed.\n";
        return;
    default:
        std::cout << "Warning! vk_allocate_command_buffer error=" << vkr << "\n";
        return;
    }
}

void my_create_source_buffer(VkDeviceSize size)
{
    vk_buffer buffer = {};
    VkResult vkr = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &buffer);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Source buffer created successfully.\n";
        vkr = vk_track_buffer(&buffer, size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (vkr == VK_SUCCESS)
        {
            std::cout << "Source buffer tracked successfully.\n";
            g_sourceBufferIndex = (int)m_buffers.size() - 1;
            // Initialize source buffer with test data
            void *data = nullptr;
            vkr = vkMapMemory(m_device, buffer.memory, 0, size, 0, &data);
            if (vkr == VK_SUCCESS && data != nullptr)
            {
                uint8_t *byteData = (uint8_t *)data;
                for (VkDeviceSize i = 0; i < size; ++i)
                    byteData[i] = (uint8_t)(i % 256);
                vkUnmapMemory(m_device, buffer.memory);
                std::cout << "Source buffer initialized with test data.\n";
            }
        }
        else
            std::cout << "Warning: Source buffer tracking failed.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Source buffer creation failed.\n";
        break;
    default:
        std::cout << "Warning! vk_create_buffer error=" << vkr << "\n";
    }
}

void my_create_staging_buffer(VkDeviceSize size)
{
    vk_buffer buffer = {};
    VkResult vkr = vk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &buffer);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Staging buffer created successfully.\n";
        vkr = vk_track_buffer(&buffer, size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkr == VK_SUCCESS)
        {
            std::cout << "Staging buffer tracked successfully.\n";
            g_stagingBufferIndex = (int)m_buffers.size() - 1;
        }
        else
            std::cout << "Warning: Staging buffer tracking failed.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Staging buffer creation failed.\n";
        break;
    default:
        std::cout << "Warning! vk_create_buffer error=" << vkr << "\n";
    }
}

void my_copy_and_readback(VkDeviceSize size)
{
    if (g_sourceBufferIndex < 0 || g_stagingBufferIndex < 0)
    {
        std::cout << "Warning: Source or staging buffer not available.\n";
        return;
    }

    vk_buffer &srcBuf = m_buffers[g_sourceBufferIndex];
    vk_buffer &stagingBuf = m_buffers[g_stagingBufferIndex];

    // Record copy command
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult vkr = vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBeginCommandBuffer failed with result=" << vkr << "\n";
        return;
    }

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;

    vkCmdCopyBuffer(m_commandBuffer, srcBuf.handle, stagingBuf.handle, 1, &copyRegion);

    vkr = vkEndCommandBuffer(m_commandBuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEndCommandBuffer failed with result=" << vkr << "\n";
        return;
    }

    // Submit and wait
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;

    vkr = vkCreateFence(m_device, &fenceInfo, &g_bufferAllocCallbacks, &fence);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateFence failed with result=" << vkr << "\n";
        return;
    }

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_commandBuffer;

    vkr = vkQueueSubmit(m_graphicsQueue, 1, &submit, fence);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkQueueSubmit failed with result=" << vkr << "\n";
        vkDestroyFence(m_device, fence, &g_bufferAllocCallbacks);
        return;
    }

    vkr = vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkWaitForFences failed with result=" << vkr << "\n";
        vkDestroyFence(m_device, fence, &g_bufferAllocCallbacks);
        return;
    }

    vkDestroyFence(m_device, fence, &g_bufferAllocCallbacks);

    // Map staging buffer and read data
    void *mapped = nullptr;
    vkr = vkMapMemory(m_device, stagingBuf.memory, 0, size, 0, &mapped);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkMapMemory failed with result=" << vkr << "\n";
        return;
    }

    if (mapped != nullptr)
    {
        uint8_t *data = (uint8_t *)mapped;
        std::cout << "Readback successful. First 16 bytes: ";
        for (uint32_t i = 0; i < 16 && i < size; ++i)
            std::cout << (int)data[i] << " ";
        std::cout << "\n";

        // Verify data matches source
        bool dataValid = true;
        for (VkDeviceSize i = 0; i < size; ++i)
        {
            if (data[i] != (uint8_t)(i % 256))
            {
                dataValid = false;
                break;
            }
        }
        if (dataValid)
            std::cout << "Data verification: PASSED - all bytes match source.\n";
        else
            std::cout << "Warning: Data verification failed.\n";
    }

    vkUnmapMemory(m_device, stagingBuf.memory);
}

// ---- dbg_* functions ----

void dbg_buffer_info(const vk_buffer *buffer)
{
    if (buffer == nullptr)
    {
        std::cout << "Buffer pointer is null.\n";
        return;
    }

    std::cout << "Buffer Info:\n";
    std::cout << "  Handle: " << buffer->handle << "\n";
    std::cout << "  Memory: " << buffer->memory << "\n";
    std::cout << "  Size: " << buffer->size << " bytes\n";
}
