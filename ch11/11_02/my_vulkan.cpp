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
VkQueue m_queue = VK_NULL_HANDLE;
std::vector<vk_semaphore> m_semaphores;
int device_count = 0;

// Host allocation callbacks for semaphore objects
static vk_allocator g_semaphoreAllocator;
static const VkAllocationCallbacks g_semaphoreAllocCallbacks = g_semaphoreAllocator;

// Command pool for command buffers
static VkCommandPool m_commandPool = VK_NULL_HANDLE;

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

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Semaphore Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    std::cout << "vkCreateInstance called\n";
    result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result=" << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::cout << "vkCreateInstance succeeded\n";

    std::cout << "vkEnumeratePhysicalDevices called\n";
    uint32_t physicalDevCount = 0;
    result = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result=" << result << "\n";
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

VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount)
{
    VkResult vkr = VK_INCOMPLETE;
    if(queueFamilyPropertyCount == nullptr || deviceIndex < 0 ||
        device_count <= deviceIndex || m_devices == nullptr)
    {
        std::cout << "Warning: Invalid device index or no device ready.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkQueueFamilyProperties* queueFamilyProperties = nullptr;
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);
    if(*queueFamilyPropertyCount == 0)
    {
        std::cout << "Warning: Device family not found.\n";
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
            std::cout << "Attempting device creation without tessellation/geometry features...\n";
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

    // Get the queue from queue family 0
    vkGetDeviceQueue(m_device, 0, 0, &m_queue);
    if (m_queue == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Failed to get device queue.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkResult vk_create_semaphore(vk_semaphore *outSemaphore)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create semaphore.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkSemaphoreCreateInfo semaphoreCreateInfo =
    {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr,
        0
    };

    vkr = vkCreateSemaphore(m_device, &semaphoreCreateInfo, &g_semaphoreAllocCallbacks, &outSemaphore->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateSemaphore failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_command_buffer(VkCommandBuffer *outCmdBuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create command buffer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create command pool if it doesn't exist
    if (m_commandPool == VK_NULL_HANDLE)
    {
        const VkCommandPoolCreateInfo poolCreateInfo =
        {
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            0
        };

        vkr = vkCreateCommandPool(m_device, &poolCreateInfo, &g_semaphoreAllocCallbacks, &m_commandPool);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkCreateCommandPool failed with result=" << vkr << "\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    const VkCommandBufferAllocateInfo allocInfo =
    {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
        m_commandPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1
    };

    vkr = vkAllocateCommandBuffers(m_device, &allocInfo, outCmdBuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkAllocateCommandBuffers failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_cleanup()
{
    for (const vk_semaphore &sem : m_semaphores)
    {
        vkDestroySemaphore(m_device, sem.handle, &g_semaphoreAllocCallbacks);
    }
    m_semaphores.clear();

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, &g_semaphoreAllocCallbacks);
        m_commandPool = VK_NULL_HANDLE;
    }

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

// ---- my_* functions ----

void my_init_vulkan()
{
#ifdef ENABLE_VALIDATION
    if (!getenv("VK_LAYER_PATH"))
    {
        _putenv_s("VK_LAYER_PATH", VK_LAYER_PATH);
        std::cout << "VK_LAYER_PATH = " << VK_LAYER_PATH << "\n";
    }
#endif

    std::cout << "Checking for physical graphics devices..\n";
    int rc = vk_device_init_count(&device_count);
    if(rc == VK_SUCCESS)
    {
        std::cout << "Found " << device_count << " physical graphics devices.\n";
    }
    else
    {
        switch(rc)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Initialization failed.\n";
            break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cout << "Out of host memory.\n";
            break;
        default:
            std::cout << "Unknown error code " << rc << "\n";
        }
    }
}

void my_run_semaphore_test()
{
    // Create two command buffers for producer and consumer
    VkCommandBuffer producerCmd, consumerCmd;
    VkResult vkr = vk_create_command_buffer(&producerCmd);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Producer command buffer could not be created.\n";
        return;
    }

    vkr = vk_create_command_buffer(&consumerCmd);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Consumer command buffer could not be created.\n";
        return;
    }

    // Create a semaphore for GPU-to-GPU synchronization
    vk_semaphore produced;
    vkr = vk_create_semaphore(&produced);
    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Semaphore object could not be created.\n";
            break;
        default:
            std::cout << "Warning! vk_create_semaphore error=" << vkr << "\n";
        }
        return;
    }

    // Record producer command buffer (minimal - just to demonstrate)
    {
        const VkCommandBufferBeginInfo beginInfo =
        {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            0, nullptr
        };

        vkr = vkBeginCommandBuffer(producerCmd, &beginInfo);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkBeginCommandBuffer failed with result=" << vkr << "\n";
            vkDestroySemaphore(m_device, produced.handle, &g_semaphoreAllocCallbacks);
            return;
        }
    }

    vkr = vkEndCommandBuffer(producerCmd);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEndCommandBuffer failed with result=" << vkr << "\n";
        vkDestroySemaphore(m_device, produced.handle, &g_semaphoreAllocCallbacks);
        return;
    }

    // Record consumer command buffer
    {
        const VkCommandBufferBeginInfo beginInfo =
        {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            0, nullptr
        };

        vkr = vkBeginCommandBuffer(consumerCmd, &beginInfo);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkBeginCommandBuffer failed with result=" << vkr << "\n";
            vkDestroySemaphore(m_device, produced.handle, &g_semaphoreAllocCallbacks);
            return;
        }
    }

    vkr = vkEndCommandBuffer(consumerCmd);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEndCommandBuffer failed with result=" << vkr << "\n";
        vkDestroySemaphore(m_device, produced.handle, &g_semaphoreAllocCallbacks);
        return;
    }

    // Submit producer with signal semaphore
    {
        const VkSubmitInfo producerSubmit =
        {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr,
            0, nullptr, nullptr,
            1, &producerCmd,
            1, &produced.handle
        };

        vkr = vkQueueSubmit(m_queue, 1, &producerSubmit, VK_NULL_HANDLE);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkQueueSubmit (producer) failed with result=" << vkr << "\n";
            vkDestroySemaphore(m_device, produced.handle, &g_semaphoreAllocCallbacks);
            return;
        }
    }

    // Submit consumer waiting on semaphore
    {
        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        const VkSubmitInfo consumerSubmit =
        {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr,
            1, &produced.handle, &waitStage,
            1, &consumerCmd,
            0, nullptr
        };

        vkr = vkQueueSubmit(m_queue, 1, &consumerSubmit, VK_NULL_HANDLE);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkQueueSubmit (consumer) failed with result=" << vkr << "\n";
            vkDestroySemaphore(m_device, produced.handle, &g_semaphoreAllocCallbacks);
            return;
        }
    }

    // Wait for queue to finish
    vkr = vkQueueWaitIdle(m_queue);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkQueueWaitIdle failed with result=" << vkr << "\n";
        vkDestroySemaphore(m_device, produced.handle, &g_semaphoreAllocCallbacks);
        return;
    }

    std::cout << "Semaphore test completed successfully!\n";

    // Clean up
    vkDestroySemaphore(m_device, produced.handle, &g_semaphoreAllocCallbacks);
}
