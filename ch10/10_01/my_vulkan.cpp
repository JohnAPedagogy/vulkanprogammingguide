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
VkCommandPool m_commandPool = VK_NULL_HANDLE;
VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
std::vector<vk_render_target> m_renderTargets;
int device_count = 0;

// Host allocation callbacks for render target objects
static vk_allocator g_renderAllocator;
static const VkAllocationCallbacks g_renderAllocCallbacks = g_renderAllocator;


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

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Ch10 Fragment Processing";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result=" << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t physicalDevCount = 0;
    result = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result=" << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDevCount);
    if (m_devices != nullptr)
    {
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
        std::cout << "Warning Graphics device not present.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);
    if(*queueFamilyPropertyCount == 0)
    {
        std::cout << "Warning Device family not found bailing...\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkr = VK_SUCCESS;
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
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_physicalDevice = m_devices[device_index];

    // Get queue handle
    vkGetDeviceQueue(m_device, 0, 0, &m_queue);

    return VK_SUCCESS;
}

VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format,
                         VkImageUsageFlags usage, vk_image *outImage)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create image.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkImageCreateInfo imageCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr,
        0,
        VK_IMAGE_TYPE_2D,
        format,
        {width, height, 1},
        1,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0, nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };

    vkr = vkCreateImage(m_device, &imageCreateInfo, &g_renderAllocCallbacks, &outImage->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImage failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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

        vkr = vkAllocateMemory(m_device, &allocInfo, &g_renderAllocCallbacks, &image->memory);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkAllocateMemory failed with result=" << vkr << "\n";
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;
        }
    }

    vkr = vkBindImageMemory(m_device, image->handle, image->memory, 0);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBindImageMemory failed with result=" << vkr << "\n";
        vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        goto free_memory;
    }

    return vkr;

free_memory:
    vkFreeMemory(m_device, image->memory, &g_renderAllocCallbacks);
    image->memory = VK_NULL_HANDLE;
destroy_image:
    vkDestroyImage(m_device, image->handle, &g_renderAllocCallbacks);
    image->handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_create_image_view(vk_image *image)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || image->handle == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid device or image handle.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkImageViewCreateInfo viewCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr,
        0,
        image->handle,
        VK_IMAGE_VIEW_TYPE_2D,
        image->format,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    vkr = vkCreateImageView(m_device, &viewCreateInfo, &g_renderAllocCallbacks, &image->view);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImageView failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_render_pass(uint32_t colorAttachmentCount, const VkFormat *formats,
                               VkRenderPass *outRenderPass)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || colorAttachmentCount == 0 || formats == nullptr)
    {
        std::cout << "Warning: Invalid device or attachment count.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Create attachment descriptions for each color attachment
    std::vector<VkAttachmentDescription> attachments(colorAttachmentCount);
    std::vector<VkAttachmentReference> colorRefs(colorAttachmentCount);

    for (uint32_t i = 0; i < colorAttachmentCount; ++i)
    {
        attachments[i] = {
            0,
            formats[i],
            VK_SAMPLE_COUNT_1_BIT,
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,
            VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            VK_ATTACHMENT_STORE_OP_DONT_CARE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
        colorRefs[i] = {i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    }

    const VkSubpassDescription subpass =
    {
        0,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0, nullptr,
        colorAttachmentCount, colorRefs.data(),
        nullptr,
        nullptr,
        0, nullptr
    };

    const VkRenderPassCreateInfo renderPassInfo =
    {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr,
        0,
        colorAttachmentCount, attachments.data(),
        1, &subpass,
        0, nullptr
    };

    vkr = vkCreateRenderPass(m_device, &renderPassInfo, &g_renderAllocCallbacks, outRenderPass);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateRenderPass failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_framebuffer(VkRenderPass renderPass, uint32_t width, uint32_t height,
                               uint32_t attachmentCount, const VkImageView *views,
                               VkFramebuffer *outFramebuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || attachmentCount == 0)
    {
        std::cout << "Warning: Invalid device, render pass, or attachment count.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkFramebufferCreateInfo framebufferInfo =
    {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr,
        0,
        renderPass,
        attachmentCount, views,
        width, height, 1
    };

    vkr = vkCreateFramebuffer(m_device, &framebufferInfo, &g_renderAllocCallbacks, outFramebuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateFramebuffer failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_command_pool(VkCommandPool *outPool)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create command pool.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkCommandPoolCreateInfo poolInfo =
    {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        0
    };

    vkr = vkCreateCommandPool(m_device, &poolInfo, &g_renderAllocCallbacks, outPool);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateCommandPool failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_allocate_command_buffer(VkCommandPool pool, VkCommandBuffer *outBuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || pool == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid device or command pool.\n";
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
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_cleanup()
{
    for (const vk_render_target &rt : m_renderTargets)
    {
        for (const vk_image &img : rt.attachments)
        {
            if (img.view != VK_NULL_HANDLE)
                vkDestroyImageView(m_device, img.view, &g_renderAllocCallbacks);
            if (img.handle != VK_NULL_HANDLE)
            {
                vkDestroyImage(m_device, img.handle, &g_renderAllocCallbacks);
                if (img.memory != VK_NULL_HANDLE)
                    vkFreeMemory(m_device, img.memory, &g_renderAllocCallbacks);
            }
        }
        if (rt.framebuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(m_device, rt.framebuffer, &g_renderAllocCallbacks);
        if (rt.renderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(m_device, rt.renderPass, &g_renderAllocCallbacks);
    }
    m_renderTargets.clear();

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, &g_renderAllocCallbacks);
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
    if(rc == VK_SUCCESS) {
        std::cout << "Found " << device_count << " physical graphics devices.\n";
    } else {
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
    default:
        std::cout << "Warning! vk_get_logical_device error=" << vkr << "\n";
    }
}

void my_create_render_target(void)
{
    const uint32_t width = 256;
    const uint32_t height = 256;
    const VkFormat formats[2] = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};

    // Create two color attachment images
    vk_image colorImg, normalImg;
    VkResult vkr = vk_create_image(width, height, formats[0],
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                   &colorImg);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create color image.\n";
        return;
    }

    vkr = vk_track_image(&colorImg, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not allocate memory for color image.\n";
        return;
    }

    vkr = vk_create_image_view(&colorImg);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create color image view.\n";
        return;
    }

    vkr = vk_create_image(width, height, formats[1],
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                          &normalImg);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create normal image.\n";
        return;
    }

    vkr = vk_track_image(&normalImg, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not allocate memory for normal image.\n";
        return;
    }

    vkr = vk_create_image_view(&normalImg);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create normal image view.\n";
        return;
    }

    // Create render pass for 2 color attachments
    VkRenderPass renderPass = VK_NULL_HANDLE;
    vkr = vk_create_render_pass(2, formats, &renderPass);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create render pass.\n";
        return;
    }

    // Create framebuffer
    VkImageView views[2] = {colorImg.view, normalImg.view};
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    vkr = vk_create_framebuffer(renderPass, width, height, 2, views, &framebuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create framebuffer.\n";
        return;
    }

    // Create command pool and buffer
    vkr = vk_create_command_pool(&m_commandPool);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create command pool.\n";
        return;
    }

    vkr = vk_allocate_command_buffer(m_commandPool, &m_commandBuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not allocate command buffer.\n";
        return;
    }

    // Store render target
    vk_render_target rt;
    rt.renderPass = renderPass;
    rt.framebuffer = framebuffer;
    rt.attachments.push_back(colorImg);
    rt.attachments.push_back(normalImg);
    m_renderTargets.push_back(rt);

    std::cout << "Fragment output render target created successfully.\n";
}
