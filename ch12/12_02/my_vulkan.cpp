#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <stddef.h>
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <string>
#include <cmath>

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkDevice m_device = VK_NULL_HANDLE;
VkQueue m_graphicsQueue = VK_NULL_HANDLE;
VkCommandPool m_commandPool = VK_NULL_HANDLE;
VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
std::vector<vk_query_pool> m_queryPools;

// Host allocation callbacks for query objects
static vk_allocator g_queryAllocator;
static const VkAllocationCallbacks g_queryAllocCallbacks = g_queryAllocator;

// Store timestamp results for processing
static uint64_t g_timestampResults[2] = {0, 0};

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
    appInfo.pApplicationName = "Chapter 12.2 - GPU Timing Queries";
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

    vkr = vkCreateInstance(&instanceCreateInfo, &g_queryAllocCallbacks, &m_instance);
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

    vkr = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, &g_queryAllocCallbacks, &m_device);
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

    vkr = vkCreateCommandPool(m_device, &poolInfo, &g_queryAllocCallbacks, &m_commandPool);
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

VkResult vk_create_timestamp_query_pool(uint32_t queryCount, vk_query_pool *outPool)
{
    VkResult vkr = VK_INCOMPLETE;

    if (outPool == nullptr)
    {
        std::cout << "Warning: outPool pointer is null.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Device not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (queryCount == 0)
    {
        std::cout << "Warning: queryCount is zero.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    {
        VkQueryPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = queryCount;

        vkr = vkCreateQueryPool(m_device, &poolInfo, &g_queryAllocCallbacks, &outPool->handle);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkCreateQueryPool failed with result=" << vkr << "\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    outPool->queryCount = queryCount;
    return vkr;
}

VkResult vk_track_query_pool(vk_query_pool *pool)
{
    VkResult vkr = VK_INCOMPLETE;

    if (pool == nullptr || pool->handle == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid query pool.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_queryPools.push_back(*pool);
    return VK_SUCCESS;
}

VkResult vk_cleanup()
{
    for (const vk_query_pool &pool : m_queryPools)
    {
        if (pool.handle != VK_NULL_HANDLE)
            vkDestroyQueryPool(m_device, pool.handle, &g_queryAllocCallbacks);
    }
    m_queryPools.clear();

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, &g_queryAllocCallbacks);
        m_commandPool = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, &g_queryAllocCallbacks);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, &g_queryAllocCallbacks);
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

void my_create_timestamp_pool(uint32_t queryCount)
{
    vk_query_pool pool = {};
    VkResult vkr = vk_create_timestamp_query_pool(queryCount, &pool);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Timestamp query pool created successfully.\n";
        vkr = vk_track_query_pool(&pool);
        if (vkr == VK_SUCCESS)
            std::cout << "Timestamp query pool tracked successfully.\n";
        else
            std::cout << "Warning: Timestamp query pool tracking failed.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Timestamp query pool creation failed.\n";
        break;
    default:
        std::cout << "Warning! vk_create_timestamp_query_pool error=" << vkr << "\n";
    }
}

void my_record_timestamps(void)
{
    if (m_queryPools.empty())
    {
        std::cout << "Warning: No query pools available.\n";
        return;
    }

    vk_query_pool *pool = &m_queryPools[0];

    // Record command buffer with timestamp operations
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult vkr = vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBeginCommandBuffer failed with result=" << vkr << "\n";
        return;
    }

    // Write timestamp at TOP_OF_PIPE stage
    vkCmdWriteTimestamp(m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pool->handle, 0);

    // Simulated work: in a real application, this would be a render pass or compute dispatch
    // Here we just have a minimal pipeline stall
    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                         0, nullptr);

    // Write timestamp at BOTTOM_OF_PIPE stage
    vkCmdWriteTimestamp(m_commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool->handle, 1);

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

    vkr = vkCreateFence(m_device, &fenceInfo, &g_queryAllocCallbacks, &fence);
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
        vkDestroyFence(m_device, fence, &g_queryAllocCallbacks);
        return;
    }

    vkr = vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkWaitForFences failed with result=" << vkr << "\n";
        vkDestroyFence(m_device, fence, &g_queryAllocCallbacks);
        return;
    }

    vkDestroyFence(m_device, fence, &g_queryAllocCallbacks);
}

void my_read_timestamp_results(void)
{
    if (m_queryPools.empty())
    {
        std::cout << "Warning: No query pools available.\n";
        return;
    }

    vk_query_pool *pool = &m_queryPools[0];

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

    // Read query results
    VkResult vkr = vkGetQueryPoolResults(m_device, pool->handle, 0, 2, sizeof(g_timestampResults),
                                          g_timestampResults, sizeof(uint64_t),
                                          VK_QUERY_RESULT_64_BIT);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkGetQueryPoolResults failed with result=" << vkr << "\n";
    }
    else
    {
        uint64_t tickDifference = g_timestampResults[1] - g_timestampResults[0];
        double nanoseconds = (double)tickDifference * properties.limits.timestampPeriod;

        std::cout << "Timestamp results:\n";
        std::cout << "  Start tick: " << g_timestampResults[0] << "\n";
        std::cout << "  End tick: " << g_timestampResults[1] << "\n";
        std::cout << "  Tick difference: " << tickDifference << "\n";
        std::cout << "  GPU time: " << nanoseconds << " ns\n";
        std::cout << "  GPU time: " << (nanoseconds / 1e6) << " ms\n";
    }
}

// ---- dbg_* functions ----

void dbg_query_pool_info(const vk_query_pool *pool)
{
    if (pool == nullptr)
    {
        std::cout << "Query pool pointer is null.\n";
        return;
    }

    std::cout << "Query Pool Info:\n";
    std::cout << "  Handle: " << pool->handle << "\n";
    std::cout << "  Query Count: " << pool->queryCount << "\n";
}
