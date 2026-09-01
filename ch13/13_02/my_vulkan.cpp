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
std::vector<vk_image> m_images;
std::vector<vk_pipeline> m_pipelines;
std::vector<vk_shader_module> m_shaderModules;
vk_renderpass m_renderPass;
vk_framebuffer m_framebuffer;
vk_descriptor_set m_lightingDescriptorSet;
int device_count = 0;

static vk_allocator g_bufferAllocator;
static const VkAllocationCallbacks g_bufferAllocCallbacks = g_bufferAllocator;

static VkDescriptorPool g_descriptorPool = VK_NULL_HANDLE;

// ---- helper functions ----

size_t count_enabled_features(const VkPhysicalDeviceFeatures *features)
{
    const VkBool32 *p = (const VkBool32 *)features;
    size_t count = 0;
    for (size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); ++i)
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

VkResult vk_load_spv(const char *spvPath, std::vector<uint32_t> *outCode)
{
    if (!spvPath || !outCode)
        return VK_ERROR_INITIALIZATION_FAILED;

    std::ifstream file(spvPath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "vk_load_spv: could not open " << spvPath << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    size_t fileSize = file.tellg();
    file.seekg(0);

    if (fileSize % 4 != 0)
    {
        std::cout << "vk_load_spv: " << spvPath << " is not 4-byte aligned\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    outCode->resize(fileSize / 4);
    file.read((char *)outCode->data(), fileSize);
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
    appInfo.pApplicationName = "Deferred Shading Example";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    std::cout << "vkCreateInstance called\n";
    result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result " << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

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

    VkQueueFamilyProperties* queueFamilyProperties = nullptr;
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);
    if(*queueFamilyPropertyCount == 0)
    {
        std::cout << "Warning Device family not found.\n";
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
    VkResult vkr = VK_INCOMPLETE;

    if (g_descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, g_descriptorPool, &g_bufferAllocCallbacks);
        g_descriptorPool = VK_NULL_HANDLE;
    }

    for (const vk_pipeline &pipe : m_pipelines)
    {
        if (pipe.layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(m_device, pipe.layout, &g_bufferAllocCallbacks);
        if (pipe.handle != VK_NULL_HANDLE)
            vkDestroyPipeline(m_device, pipe.handle, &g_bufferAllocCallbacks);
    }
    m_pipelines.clear();

    for (const vk_shader_module &shader : m_shaderModules)
    {
        if (shader.handle != VK_NULL_HANDLE)
            vkDestroyShaderModule(m_device, shader.handle, &g_bufferAllocCallbacks);
    }
    m_shaderModules.clear();

    if (m_framebuffer.handle != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(m_device, m_framebuffer.handle, &g_bufferAllocCallbacks);
        m_framebuffer.handle = VK_NULL_HANDLE;
    }

    if (m_renderPass.handle != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_renderPass.handle, &g_bufferAllocCallbacks);
        m_renderPass.handle = VK_NULL_HANDLE;
    }

    for (const vk_image &img : m_images)
    {
        if (img.view != VK_NULL_HANDLE)
            vkDestroyImageView(m_device, img.view, &g_bufferAllocCallbacks);
        if (img.memory != VK_NULL_HANDLE)
            vkFreeMemory(m_device, img.memory, &g_bufferAllocCallbacks);
        if (img.handle != VK_NULL_HANDLE)
            vkDestroyImage(m_device, img.handle, &g_bufferAllocCallbacks);
    }
    m_images.clear();

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

    vkGetPhysicalDeviceFeatures(m_devices[device_index], &supportedFeatures);
    requiredFeatures.multiDrawIndirect = supportedFeatures.multiDrawIndirect;
    requiredFeatures.tessellationShader = supportedFeatures.tessellationShader;
    requiredFeatures.geometryShader = supportedFeatures.geometryShader;

    const float queuePriority = 1.0f;
    const VkDeviceQueueCreateInfo deviceQueueCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        nullptr, 0, 0, 1, &queuePriority
    };
    *feature_count = (int)count_enabled_features(&supportedFeatures);
    const VkDeviceCreateInfo deviceCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        nullptr, 0, 1, &deviceQueueCreateInfo,
        0, nullptr, 0, nullptr, &requiredFeatures
    };

    if (m_devices[device_index] == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Physical device handle is null.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_physicalDevice = m_devices[device_index];
    result = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);

    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateDevice failed with result " << result << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetDeviceQueue(m_device, 0, 0, &m_queue);
    std::cout << "Logical device created and queue retrieved.\n";
    return VK_SUCCESS;
}

VkResult vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, vk_buffer *outBuffer)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE || !outBuffer)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkBufferCreateInfo bufferInfo =
    {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr
    };

    vkr = vkCreateBuffer(m_device, &bufferInfo, &g_bufferAllocCallbacks, &outBuffer->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateBuffer failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
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

VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, vk_image *outImage)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE || !outImage)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkImageCreateInfo imageInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0,
        VK_IMAGE_TYPE_2D, format, {width, height, 1},
        1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
        usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, VK_IMAGE_LAYOUT_UNDEFINED
    };

    vkr = vkCreateImage(m_device, &imageInfo, &g_bufferAllocCallbacks, &outImage->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImage failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    outImage->extent = {width, height};
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
            memRequirements.size, memoryTypeIndex
        };

        vkr = vkAllocateMemory(m_device, &allocInfo, &g_bufferAllocCallbacks, &image->memory);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkAllocateMemory failed for image with result=" << vkr << "\n";
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

    {
        const VkImageViewCreateInfo viewInfo =
        {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0,
            image->handle, VK_IMAGE_VIEW_TYPE_2D, image->format,
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };

        vkr = vkCreateImageView(m_device, &viewInfo, &g_bufferAllocCallbacks, &image->view);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkCreateImageView failed with result=" << vkr << "\n";
            vkr = VK_ERROR_INITIALIZATION_FAILED;
            goto free_memory;
        }
    }

    m_images.push_back(*image);
    return vkr;

free_memory:
    vkFreeMemory(m_device, image->memory, &g_bufferAllocCallbacks);
    image->memory = VK_NULL_HANDLE;
destroy_image:
    vkDestroyImage(m_device, image->handle, &g_bufferAllocCallbacks);
    image->handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_create_shader_module(const char *spvPath, vk_shader_module *outModule)
{
    VkResult vkr = VK_INCOMPLETE;
    if (!spvPath || !outModule || m_device == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<uint32_t> code;
    vkr = vk_load_spv(spvPath, &code);
    if (vkr != VK_SUCCESS)
        return vkr;

    const VkShaderModuleCreateInfo createInfo =
    {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
        code.size() * 4, code.data()
    };

    vkr = vkCreateShaderModule(m_device, &createInfo, &g_bufferAllocCallbacks, &outModule->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateShaderModule failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    m_shaderModules.push_back(*outModule);
    return vkr;
}

VkResult vk_create_renderpass_deferred(VkRenderPass *outRenderPass)
{
    VkResult vkr = VK_INCOMPLETE;
    if (!outRenderPass || m_device == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    // G-buffer attachments: albedo, normal, depth, and final output
    const VkAttachmentDescription attachments[4] =
    {
        // Attachment 0: albedo (G-buffer)
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
         VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        // Attachment 1: normal (G-buffer)
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
         VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        // Attachment 2: depth (G-buffer)
        {0, VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
         VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        // Attachment 3: final output
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
         VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };

    // Subpass 0: geometry pass writes G-buffer attachments
    const VkAttachmentReference gBufferRefs[3] =
    {
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };
    const VkSubpassDescription subpass0 =
    {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 3, gBufferRefs, nullptr, nullptr, 0, nullptr};

    // Subpass 1: lighting pass reads G-buffer as input attachments, writes to output
    const VkAttachmentReference inputRefs[3] =
    {
        {0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
    };
    const VkAttachmentReference outputRef = {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkSubpassDescription subpass1 =
    {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 3, inputRefs, 1, &outputRef, nullptr, nullptr, 0, nullptr};

    const VkSubpassDescription subpasses[2] = {subpass0, subpass1};

    // Dependencies between subpasses
    const VkSubpassDependency dependencies[2] =
    {
        // Geometry to lighting: write completion before read
        {0, 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT},
        // External to geometry: ready at start
        {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0}
    };

    const VkRenderPassCreateInfo renderPassInfo =
    {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0,
        4, attachments, 2, subpasses, 2, dependencies
    };

    vkr = vkCreateRenderPass(m_device, &renderPassInfo, &g_bufferAllocCallbacks, outRenderPass);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateRenderPass failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_create_framebuffer(VkRenderPass renderPass, VkFramebuffer *outFramebuffer)
{
    VkResult vkr = VK_INCOMPLETE;
    if (!outFramebuffer || m_device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    if (m_images.size() < 4)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkImageView attachmentViews[4] = {
        m_images[0].view, m_images[1].view, m_images[2].view, m_images[3].view
    };

    const VkFramebufferCreateInfo framebufferInfo =
    {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0,
        renderPass, 4, attachmentViews, 256, 256, 1
    };

    vkr = vkCreateFramebuffer(m_device, &framebufferInfo, &g_bufferAllocCallbacks, outFramebuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateFramebuffer failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_create_graphics_pipeline(VkShaderModule vertModule, VkShaderModule fragModule,
    VkRenderPass renderPass, uint32_t subpassIndex, vk_pipeline *outPipeline)
{
    VkResult vkr = VK_INCOMPLETE;
    if (!outPipeline || m_device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkPipelineShaderStageCreateInfo shaderStages[2] =
    {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
         VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr}
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputInfo =
    {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 0, nullptr, 0, nullptr};

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly =
    {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

    const VkViewport viewport = {0, 0, 256, 256, 0, 1};
    const VkRect2D scissor = {{0, 0}, {256, 256}};
    const VkPipelineViewportStateCreateInfo viewportState =
    {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, &viewport, 1, &scissor};

    const VkPipelineRasterizationStateCreateInfo rasterizer =
    {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0,
     VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
     VK_FRONT_FACE_CLOCKWISE, VK_FALSE, 0, 0, 0, 1};

    const VkPipelineMultisampleStateCreateInfo multisampling =
    {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0,
     VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 1, nullptr, VK_FALSE, VK_FALSE};

    const VkPipelineColorBlendAttachmentState colorBlendAttachment =
    {VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
     VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
     VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo colorBlending =
    {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0,
     VK_FALSE, VK_LOGIC_OP_COPY, 1, &colorBlendAttachment, {0, 0, 0, 0}};

    const VkPipelineLayoutCreateInfo pipelineLayoutInfo =
    {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 0, nullptr};

    vkr = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, &g_bufferAllocCallbacks, &outPipeline->layout);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreatePipelineLayout failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkGraphicsPipelineCreateInfo pipelineInfo =
    {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0,
     2, shaderStages, &vertexInputInfo, &inputAssembly, nullptr,
     &viewportState, &rasterizer, &multisampling, nullptr, &colorBlending,
     nullptr, outPipeline->layout, renderPass, subpassIndex, VK_NULL_HANDLE, 0};

    vkr = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, &g_bufferAllocCallbacks, &outPipeline->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateGraphicsPipelines failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        goto destroy_layout;
    }
    m_pipelines.push_back(*outPipeline);
    return vkr;

destroy_layout:
    vkDestroyPipelineLayout(m_device, outPipeline->layout, &g_bufferAllocCallbacks);
    outPipeline->layout = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_create_descriptor_set_layout(VkDescriptorSetLayout *outLayout)
{
    VkResult vkr = VK_INCOMPLETE;
    if (!outLayout || m_device == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    // Three input attachments for G-buffer (albedo, normal, depth)
    const VkDescriptorSetLayoutBinding bindings[3] =
    {
        {0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
    };

    const VkDescriptorSetLayoutCreateInfo layoutInfo =
    {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 3, bindings};

    vkr = vkCreateDescriptorSetLayout(m_device, &layoutInfo, &g_bufferAllocCallbacks, outLayout);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDescriptorSetLayout failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_allocate_descriptor_set(VkDescriptorSetLayout layout, vk_descriptor_set *outSet)
{
    VkResult vkr = VK_INCOMPLETE;
    if (!outSet || m_device == VK_NULL_HANDLE || layout == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    // Create descriptor pool if not already created
    if (g_descriptorPool == VK_NULL_HANDLE)
    {
        const VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 3};
        const VkDescriptorPoolCreateInfo poolInfo =
        {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 1, &poolSize};

        vkr = vkCreateDescriptorPool(m_device, &poolInfo, &g_bufferAllocCallbacks, &g_descriptorPool);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkCreateDescriptorPool failed with result=" << vkr << "\n";
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    const VkDescriptorSetAllocateInfo allocInfo =
    {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, g_descriptorPool, 1, &layout};

    vkr = vkAllocateDescriptorSets(m_device, &allocInfo, &outSet->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkAllocateDescriptorSets failed with result=" << vkr << "\n";
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    return vkr;
}

VkResult vk_update_descriptor_set(vk_descriptor_set *set, VkImageView inputAttachmentView)
{
    VkResult vkr = VK_INCOMPLETE;
    if (!set || m_device == VK_NULL_HANDLE || inputAttachmentView == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    // Placeholder: in full implementation, would update all three input attachment descriptors
    std::cout << "Descriptor set update: skipped (requires full image view setup).\n";
    return VK_SUCCESS;
}

// ---- my_* functions ----

void my_init_vulkan(void)
{
    VkResult vkr = vk_device_init_count(&device_count);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Device initialization succeeded.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Device initialization failed.\n";
        break;
    default:
        std::cout << "Warning! vk_device_init_count error=" << vkr << "\n";
    }
}

void my_get_device_properties(int deviceIndex)
{
    uint32_t queueFamilyPropertyCount = 0;
    VkResult vkr = vk_get_device_properties(deviceIndex, &queueFamilyPropertyCount);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Device has " << queueFamilyPropertyCount << " queue families.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not get device properties.\n";
        break;
    default:
        std::cout << "Warning! vk_get_device_properties error=" << vkr << "\n";
    }
}

void my_get_logical_device(int deviceIndex)
{
    int feature_count = 0;
    VkResult vkr = vk_get_logical_device(deviceIndex, &feature_count);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Logical device created with " << feature_count << " features.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create logical device.\n";
        break;
    default:
        std::cout << "Warning! vk_get_logical_device error=" << vkr << "\n";
    }
}

void my_create_gbuffer_images(void)
{
    // Create four images: albedo, normal, depth (all for G-buffer) + final output
    VkFormat formats[4] = {
        VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM
    };

    for (int i = 0; i < 4; ++i)
    {
        vk_image img = {};
        VkResult vkr = vk_create_image(256, 256, formats[i],
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, &img);
        switch (vkr)
        {
        case VK_SUCCESS:
            vkr = vk_track_image(&img, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            switch (vkr)
            {
            case VK_SUCCESS:
                std::cout << "G-buffer image " << i << " created.\n";
                break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY:
                std::cout << "Warning: Out of device memory for image " << i << ".\n";
                break;
            default:
                std::cout << "Warning! vk_track_image error=" << vkr << "\n";
            }
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Could not create image " << i << ".\n";
            break;
        default:
            std::cout << "Warning! vk_create_image error=" << vkr << "\n";
        }
    }
}

void my_create_shader_modules(void)
{
    std::cout << "Shader modules: skipped (would load from .spv files).\n";
}

void my_create_renderpass(void)
{
    VkResult vkr = vk_create_renderpass_deferred(&m_renderPass.handle);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Deferred render pass created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create render pass.\n";
        break;
    default:
        std::cout << "Warning! vk_create_renderpass error=" << vkr << "\n";
    }
}

void my_create_framebuffer(void)
{
    VkResult vkr = vk_create_framebuffer(m_renderPass.handle, &m_framebuffer.handle);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Framebuffer created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not create framebuffer.\n";
        break;
    default:
        std::cout << "Warning! vk_create_framebuffer error=" << vkr << "\n";
    }
}

void my_create_pipelines(void)
{
    std::cout << "Graphics pipelines: skipped (requires compiled shaders).\n";
}

void my_create_descriptor_sets(void)
{
    std::cout << "Descriptor sets: skipped (requires G-buffer images linked).\n";
}

void my_run_deferred_pass(void)
{
    std::cout << "Deferred rendering pass: skipped (requires complete setup).\n";
}

void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    if (!p || count <= 0) return;
    for (int i = 0; i < count; ++i)
        std::cout << "  Layer " << i << ": " << p[i].layerName << "\n";
}
