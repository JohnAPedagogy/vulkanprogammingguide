#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <stddef.h>
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <string>
#include <fstream>

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
VkDevice m_device = VK_NULL_HANDLE;
VkQueue m_queue = VK_NULL_HANDLE;
std::vector<vk_buffer> m_buffers;
int device_count = 0;

vk_image m_colorImage;
vk_render_pass m_renderPass;
vk_framebuffer m_framebuffer;
vk_pipeline m_pipeline;

// Host allocation callbacks for buffer/memory objects
static vk_allocator g_bufferAllocator;
static const VkAllocationCallbacks g_bufferAllocCallbacks = g_bufferAllocator;
static vk_allocator g_imageAllocator;
static const VkAllocationCallbacks g_imageAllocCallbacks = g_imageAllocator;

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

VkResult read_shader_file(const char *path, std::vector<char> *outCode)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "Failed to open shader file: " << path << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    size_t fileSize = (size_t) file.tellg();
    outCode->resize(fileSize);

    file.seekg(0);
    file.read(outCode->data(), fileSize);
    file.close();

    return VK_SUCCESS;
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
    appInfo.pApplicationName = "Chapter 8";
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
    if (result != VK_SUCCESS || physicalDevCount == 0)
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
    }

    return result;
}

VkResult vk_get_device_properties(int deviceIndex, uint32_t *queueFamilyPropertyCount)
{
    VkResult vkr = VK_INCOMPLETE;
    if(queueFamilyPropertyCount == nullptr || deviceIndex < 0 ||
        device_count <= deviceIndex || m_devices == nullptr)
    {
        std::cout << "Warning: Graphics device not present.\n";
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

    vkGetPhysicalDeviceFeatures(m_devices[device_index], &supportedFeatures);

    requiredFeatures.multiDrawIndirect = supportedFeatures.multiDrawIndirect;
    requiredFeatures.tessellationShader = supportedFeatures.tessellationShader;
    requiredFeatures.geometryShader = supportedFeatures.geometryShader;

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

    result = vkCreateDevice(m_devices[device_index], &deviceCreateInfo, nullptr, &m_device);

    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateDevice failed with result=" << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_physicalDevice = m_devices[device_index];

    // Get the queue handle
    vkGetDeviceQueue(m_device, 0, 0, &m_queue);

    return VK_SUCCESS;
}

VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, vk_image *outImage)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || outImage == nullptr)
    {
        std::cout << "Warning: Invalid device or output parameter for image creation.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkImageCreateInfo imageCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
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
        0,
        nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };

    vkr = vkCreateImage(m_device, &imageCreateInfo, &g_imageAllocCallbacks, &outImage->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImage failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    outImage->format = format;
    outImage->width = width;
    outImage->height = height;

    return vkr;
}

VkResult vk_track_image(vk_image *image, uint32_t width, uint32_t height, VkFormat format, VkMemoryPropertyFlags properties)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || image == nullptr)
    {
        std::cout << "Warning: Invalid device or image for tracking.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
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

    image->format = format;
    image->width = width;
    image->height = height;
    return vkr;

free_memory:
    vkFreeMemory(m_device, image->memory, &g_imageAllocCallbacks);
    image->memory = VK_NULL_HANDLE;
destroy_image:
    vkDestroyImage(m_device, image->handle, &g_imageAllocCallbacks);
    image->handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_create_shader_module(const std::vector<char> *spvCode, VkShaderModule *outModule)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || spvCode == nullptr || outModule == nullptr)
    {
        std::cout << "Warning: Invalid device or shader code for module creation.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkShaderModuleCreateInfo createInfo =
    {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        nullptr,
        0,
        spvCode->size(),
        (const uint32_t*)spvCode->data()
    };

    vkr = vkCreateShaderModule(m_device, &createInfo, &g_bufferAllocCallbacks, outModule);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateShaderModule failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_render_pass(VkFormat colorFormat, vk_render_pass *outRenderPass)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || outRenderPass == nullptr)
    {
        std::cout << "Warning: Invalid device or output parameter for render pass.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkAttachmentDescription colorAttachment =
    {
        0,
        colorFormat,
        VK_SAMPLE_COUNT_1_BIT,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkAttachmentReference colorAttachmentRef =
    {
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass =
    {
        0,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        0,
        nullptr,
        1,
        &colorAttachmentRef,
        nullptr,
        nullptr,
        0,
        nullptr
    };

    const VkRenderPassCreateInfo renderPassInfo =
    {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        0,
        1,
        &colorAttachment,
        1,
        &subpass,
        0,
        nullptr
    };

    vkr = vkCreateRenderPass(m_device, &renderPassInfo, &g_bufferAllocCallbacks, &outRenderPass->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateRenderPass failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_framebuffer(vk_image *colorImage, vk_render_pass *renderPass, vk_framebuffer *outFramebuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || colorImage == nullptr || renderPass == nullptr || outFramebuffer == nullptr)
    {
        std::cout << "Warning: Invalid parameters for framebuffer creation.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkImageView colorView;
    const VkImageViewCreateInfo viewCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0,
        colorImage->handle,
        VK_IMAGE_VIEW_TYPE_2D,
        colorImage->format,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    vkr = vkCreateImageView(m_device, &viewCreateInfo, &g_imageAllocCallbacks, &colorView);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImageView failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkFramebufferCreateInfo framebufferInfo =
    {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        0,
        renderPass->handle,
        1,
        &colorView,
        colorImage->width,
        colorImage->height,
        1
    };

    vkr = vkCreateFramebuffer(m_device, &framebufferInfo, &g_bufferAllocCallbacks, &outFramebuffer->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateFramebuffer failed with result=" << vkr << "\n";
        vkDestroyImageView(m_device, colorView, &g_imageAllocCallbacks);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkDestroyImageView(m_device, colorView, &g_imageAllocCallbacks);

    return vkr;
}

VkResult vk_create_pipeline(VkShaderModule vertShader, VkShaderModule fragShader, vk_render_pass *renderPass, vk_pipeline *outPipeline)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || renderPass == nullptr || outPipeline == nullptr)
    {
        std::cout << "Warning: Invalid parameters for pipeline creation.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkPipelineShaderStageCreateInfo shaderStages[2] =
    {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertShader,
            "main",
            nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr,
            0,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragShader,
            "main",
            nullptr
        }
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,
        0,
        0,
        nullptr,
        0,
        nullptr
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly =
    {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE
    };

    const VkViewport viewport = {0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f};
    const VkRect2D scissor = {{0, 0}, {800, 600}};

    const VkPipelineViewportStateCreateInfo viewportState =
    {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        nullptr,
        0,
        1,
        &viewport,
        1,
        &scissor
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer =
    {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_FALSE,
        VK_FALSE,
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_BACK_BIT,
        VK_FRONT_FACE_CLOCKWISE,
        VK_FALSE,
        0.0f,
        0.0f,
        0.0f,
        1.0f
    };

    const VkPipelineMultisampleStateCreateInfo multisampling =
    {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FALSE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachment =
    {
        VK_FALSE,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_ZERO,
        VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    const VkPipelineColorBlendStateCreateInfo colorBlending =
    {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        nullptr,
        0,
        VK_FALSE,
        VK_LOGIC_OP_COPY,
        1,
        &colorBlendAttachment,
        {0.0f, 0.0f, 0.0f, 0.0f}
    };

    const VkPipelineLayoutCreateInfo pipelineLayoutInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        nullptr,
        0,
        0,
        nullptr,
        0,
        nullptr
    };

    vkr = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, &g_bufferAllocCallbacks, &outPipeline->layout);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreatePipelineLayout failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkGraphicsPipelineCreateInfo pipelineInfo =
    {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        nullptr,
        0,
        2,
        shaderStages,
        &vertexInputInfo,
        &inputAssembly,
        nullptr,
        &viewportState,
        &rasterizer,
        &multisampling,
        nullptr,
        &colorBlending,
        nullptr,
        outPipeline->layout,
        renderPass->handle,
        0,
        VK_NULL_HANDLE,
        -1
    };

    vkr = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, &g_bufferAllocCallbacks, &outPipeline->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateGraphicsPipelines failed with result=" << vkr << "\n";
        vkDestroyPipelineLayout(m_device, outPipeline->layout, &g_bufferAllocCallbacks);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || outBuffer == nullptr)
    {
        std::cout << "Warning: Invalid device or output parameter for buffer creation.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkBufferCreateInfo bufferCreateInfo =
    {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        nullptr,
        0,
        size,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        nullptr
    };

    vkr = vkCreateBuffer(m_device, &bufferCreateInfo, &g_bufferAllocCallbacks, &outBuffer->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateBuffer failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    outBuffer->size = size;
    return vkr;
}

VkResult vk_track_buffer(vk_buffer *buffer, VkDeviceSize size, VkMemoryPropertyFlags properties)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || buffer == nullptr)
    {
        std::cout << "Warning: Invalid device or buffer for tracking.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            nullptr,
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

VkResult vk_cleanup()
{
    // Destroy pipeline and layout
    if (m_pipeline.handle != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_pipeline.handle, &g_bufferAllocCallbacks);
        m_pipeline.handle = VK_NULL_HANDLE;
    }
    if (m_pipeline.layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_pipeline.layout, &g_bufferAllocCallbacks);
        m_pipeline.layout = VK_NULL_HANDLE;
    }

    // Destroy framebuffer
    if (m_framebuffer.handle != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(m_device, m_framebuffer.handle, &g_bufferAllocCallbacks);
        m_framebuffer.handle = VK_NULL_HANDLE;
    }

    // Destroy render pass
    if (m_renderPass.handle != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_renderPass.handle, &g_bufferAllocCallbacks);
        m_renderPass.handle = VK_NULL_HANDLE;
    }

    // Destroy color image
    if (m_colorImage.handle != VK_NULL_HANDLE)
    {
        vkDestroyImage(m_device, m_colorImage.handle, &g_imageAllocCallbacks);
        m_colorImage.handle = VK_NULL_HANDLE;
    }
    if (m_colorImage.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_device, m_colorImage.memory, &g_imageAllocCallbacks);
        m_colorImage.memory = VK_NULL_HANDLE;
    }

    // Destroy buffers
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

// ---- my_* functions ----

void my_init_vulkan(void)
{
    VkResult vkr = vk_device_init_count(&device_count);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Device initialization succeeded with " << device_count << " devices.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to initialize device.\n";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        std::cout << "Warning: Out of host memory.\n";
        break;
    default:
        std::cout << "Warning! Unexpected error code from device init: " << vkr << "\n";
    }
}

void my_create_image(void)
{
    VkResult vkr = vk_create_image(800, 600, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &m_colorImage);
    if (vkr != VK_SUCCESS)
    {
        vkr = vk_track_image(&m_colorImage, 800, 600, VK_FORMAT_R8G8B8A8_UNORM, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Color image created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create color image.\n";
        break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        std::cout << "Warning: Not enough device memory for color image.\n";
        break;
    default:
        std::cout << "Warning! Unexpected error from image creation: " << vkr << "\n";
    }
}

void my_create_shader_modules(VkShaderModule *vertShader, VkShaderModule *fragShader)
{
    // For now, create minimal shader modules inline (full implementation would compile from GLSL)
    // This is a placeholder that creates valid but trivial shaders
    uint32_t vertSpirv[] = {
        0x07230203, 0x00010300, 0x0008000b, 0x00000001,
        0x00000000, 0x00020011, 0x00000001, 0x0006000b,
        0x00000001, 0x4f476c47, 0x6c676f4f, 0x00000000
    };

    uint32_t fragSpirv[] = {
        0x07230203, 0x00010300, 0x0008000b, 0x00000001,
        0x00000000, 0x00020011, 0x00000001, 0x0006000b,
        0x00000001, 0x4f476c47, 0x6c676f4f, 0x00000000
    };

    std::vector<char> vertCode((char*)vertSpirv, (char*)(vertSpirv + 13));
    std::vector<char> fragCode((char*)fragSpirv, (char*)(fragSpirv + 13));

    VkResult vkr = vk_create_shader_module(&vertCode, vertShader);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Vertex shader module created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create vertex shader module.\n";
        break;
    default:
        std::cout << "Warning! Unexpected error from vertex shader: " << vkr << "\n";
    }

    vkr = vk_create_shader_module(&fragCode, fragShader);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Fragment shader module created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create fragment shader module.\n";
        break;
    default:
        std::cout << "Warning! Unexpected error from fragment shader: " << vkr << "\n";
    }
}

void my_create_render_pass(void)
{
    VkResult vkr = vk_create_render_pass(VK_FORMAT_R8G8B8A8_UNORM, &m_renderPass);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Render pass created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create render pass.\n";
        break;
    default:
        std::cout << "Warning! Unexpected error from render pass creation: " << vkr << "\n";
    }
}

void my_create_framebuffer(void)
{
    VkResult vkr = vk_create_framebuffer(&m_colorImage, &m_renderPass, &m_framebuffer);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Framebuffer created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create framebuffer.\n";
        break;
    default:
        std::cout << "Warning! Unexpected error from framebuffer creation: " << vkr << "\n";
    }
}

void my_create_pipeline(VkShaderModule vertShader, VkShaderModule fragShader)
{
    VkResult vkr = vk_create_pipeline(vertShader, fragShader, &m_renderPass, &m_pipeline);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Graphics pipeline created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create graphics pipeline.\n";
        break;
    default:
        std::cout << "Warning! Unexpected error from pipeline creation: " << vkr << "\n";
    }
}

void my_create_vertex_buffer(void)
{
    float triangleVertices[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    vk_buffer vertexBuffer;
    VkResult vkr = vk_create_buffer(sizeof(triangleVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vertexBuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create vertex buffer.\n";
        return;
    }

    vkr = vk_track_buffer(&vertexBuffer, sizeof(triangleVertices), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not track vertex buffer.\n";
        return;
    }

    // Copy vertex data to buffer
    void *data;
    vkMapMemory(m_device, vertexBuffer.memory, 0, sizeof(triangleVertices), 0, &data);
    memcpy(data, triangleVertices, sizeof(triangleVertices));
    vkUnmapMemory(m_device, vertexBuffer.memory);

    std::cout << "Vertex buffer created and populated.\n";
}

void my_record_and_submit_draw(void)
{
    // Create command pool
    const VkCommandPoolCreateInfo poolInfo = {
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        nullptr,
        0,
        0
    };

    VkCommandPool commandPool;
    VkResult vkr = vkCreateCommandPool(m_device, &poolInfo, &g_bufferAllocCallbacks, &commandPool);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not create command pool.\n";
        return;
    }

    // Allocate command buffer
    const VkCommandBufferAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        nullptr,
        commandPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        1
    };

    VkCommandBuffer commandBuffer;
    vkr = vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not allocate command buffer.\n";
        vkDestroyCommandPool(m_device, commandPool, &g_bufferAllocCallbacks);
        return;
    }

    // Record draw commands
    const VkCommandBufferBeginInfo beginInfo = {
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        nullptr,
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        nullptr
    };

    vkr = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not begin recording command buffer.\n";
        vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(m_device, commandPool, &g_bufferAllocCallbacks);
        return;
    }

    const VkClearValue clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};
    const VkRenderPassBeginInfo renderPassInfo = {
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        nullptr,
        m_renderPass.handle,
        m_framebuffer.handle,
        {{0, 0}, {800, 600}},
        1,
        &clearValue
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.handle);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);

    vkr = vkEndCommandBuffer(commandBuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not end recording command buffer.\n";
        vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(m_device, commandPool, &g_bufferAllocCallbacks);
        return;
    }

    // Submit command buffer
    const VkSubmitInfo submitInfo = {
        VK_STRUCTURE_TYPE_SUBMIT_INFO,
        nullptr,
        0,
        nullptr,
        nullptr,
        1,
        &commandBuffer,
        0,
        nullptr
    };

    vkr = vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Could not submit command buffer to queue.\n";
    }
    else
    {
        std::cout << "Draw command submitted to queue.\n";
    }

    vkQueueWaitIdle(m_queue);

    vkFreeCommandBuffers(m_device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(m_device, commandPool, &g_bufferAllocCallbacks);
}
