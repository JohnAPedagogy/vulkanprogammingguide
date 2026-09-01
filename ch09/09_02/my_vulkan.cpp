#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <stddef.h>
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <fstream>
#include <vector>
#include <cstring>

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkDevice m_device = VK_NULL_HANDLE;
VkQueue m_queue = VK_NULL_HANDLE;
uint32_t m_queueFamilyIndex = 0;

std::vector<vk_shader_module> m_shaderModules;
std::vector<vk_buffer> m_buffers;
vk_render_pass m_renderPass = {};
vk_framebuffer m_framebuffer = {};
vk_pipeline m_pipeline = {};
vk_command_buffer m_commandBuffer = {};
vk_image m_outputImage = {};
vk_image m_heightMap = {};
vk_sampler m_sampler = {};
vk_descriptor_set_layout m_descriptorSetLayout = {};
vk_descriptor_set m_descriptorSet = {};

// Host allocation callbacks
static vk_allocator g_allocator;
static const VkAllocationCallbacks g_allocCallbacks = g_allocator;

static VkDescriptorPool g_descriptorPool = VK_NULL_HANDLE;

// ---- helper functions ----

size_t count_enabled_features(const VkPhysicalDeviceFeatures *features)
{
    const VkBool32 *p = (const VkBool32 *)features;
    size_t count = 0;
    for (size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); ++i)
    {
        if (p[i]) ++count;
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

static std::vector<char> read_spv_file(const char *path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cout << "Warning: Cannot open SPIR-V file: " << path << "\n";
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size))
    {
        std::cout << "Warning: Cannot read SPIR-V file: " << path << "\n";
        return {};
    }
    return buffer;
}

// ---- vk_* functions ----

VkResult vk_init_instance()
{
    VkResult vkr = VK_INCOMPLETE;

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Displacement Mapping",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .apiVersion = VK_API_VERSION_1_0
    };

    const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = validationLayers
    };

    vkr = vkCreateInstance(&createInfo, &g_allocCallbacks, &m_instance);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_get_physical_device()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_instance == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No instance, cannot enumerate devices.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t deviceCount = 0;
    vkr = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (vkr != VK_SUCCESS || deviceCount == 0)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkr = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_physicalDevice = devices[0];
    return vkr;
}

VkResult vk_create_device()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No physical device, cannot create logical device.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
    {
        std::cout << "Warning: No queue families found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    m_queueFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            m_queueFamilyIndex = i;
            break;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.tessellationShader = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledFeatureCount = 0,
        .pEnabledFeatures = &deviceFeatures
    };

    vkr = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, &g_allocCallbacks, &m_device);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDevice failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return vkr;
}

VkResult vk_get_queue()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No device, cannot get queue.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
    if (m_queue == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Failed to get queue.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult vk_create_command_pool_and_buffer()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No device, cannot create command pool.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = m_queueFamilyIndex,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };

    VkCommandPool commandPool = VK_NULL_HANDLE;
    vkr = vkCreateCommandPool(m_device, &poolCreateInfo, &g_allocCallbacks, &commandPool);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateCommandPool failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    vkr = vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkAllocateCommandBuffers failed with result=" << vkr << "\n";
        vkDestroyCommandPool(m_device, commandPool, &g_allocCallbacks);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_shader_module(const char *spvPath, vk_shader_module *outModule)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || outModule == nullptr)
    {
        std::cout << "Warning: Invalid device or output module.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<char> spvData = read_spv_file(spvPath);
    if (spvData.empty())
    {
        std::cout << "Warning: SPIR-V file reading failed: " << spvPath << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spvData.size(),
        .pCode = reinterpret_cast<const uint32_t *>(spvData.data())
    };

    vkr = vkCreateShaderModule(m_device, &createInfo, &g_allocCallbacks, &outModule->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateShaderModule failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    m_shaderModules.push_back(*outModule);
    return vkr;
}

VkResult vk_create_render_pass()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No device, cannot create render pass.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkAttachmentDescription colorAttachment = {
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };

    VkRenderPassCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass
    };

    vkr = vkCreateRenderPass(m_device, &createInfo, &g_allocCallbacks, &m_renderPass.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateRenderPass failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_output_image()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || m_physicalDevice == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid device/physical device.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {256, 256, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    vkr = vkCreateImage(m_device, &imageCreateInfo, &g_allocCallbacks, &m_outputImage.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImage failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_outputImage.handle, &memRequirements);

    uint32_t memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (memoryTypeIndex == UINT32_MAX)
    {
        std::cout << "Warning: No suitable memory type for image.\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        goto destroy_image;
    }

    {
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memoryTypeIndex
        };

        vkr = vkAllocateMemory(m_device, &allocInfo, &g_allocCallbacks, &m_outputImage.memory);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkAllocateMemory failed with result=" << vkr << "\n";
            vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
            goto destroy_image;
        }
    }

    vkr = vkBindImageMemory(m_device, m_outputImage.handle, m_outputImage.memory, 0);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBindImageMemory failed with result=" << vkr << "\n";
        vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        goto free_memory;
    }

    {
        VkImageViewCreateInfo viewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_outputImage.handle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        vkr = vkCreateImageView(m_device, &viewCreateInfo, &g_allocCallbacks, &m_outputImage.view);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkCreateImageView failed with result=" << vkr << "\n";
            vkr = VK_ERROR_INITIALIZATION_FAILED;
            goto free_memory;
        }
    }

    return vkr;

free_memory:
    vkFreeMemory(m_device, m_outputImage.memory, &g_allocCallbacks);
    m_outputImage.memory = VK_NULL_HANDLE;
destroy_image:
    vkDestroyImage(m_device, m_outputImage.handle, &g_allocCallbacks);
    m_outputImage.handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_create_height_map_image()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || m_physicalDevice == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid device/physical device.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .extent = {256, 256, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    vkr = vkCreateImage(m_device, &imageCreateInfo, &g_allocCallbacks, &m_heightMap.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImage failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, m_heightMap.handle, &memRequirements);

    uint32_t memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (memoryTypeIndex == UINT32_MAX)
    {
        std::cout << "Warning: No suitable memory type for height map.\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        goto destroy_image;
    }

    {
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memoryTypeIndex
        };

        vkr = vkAllocateMemory(m_device, &allocInfo, &g_allocCallbacks, &m_heightMap.memory);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkAllocateMemory failed with result=" << vkr << "\n";
            vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
            goto destroy_image;
        }
    }

    vkr = vkBindImageMemory(m_device, m_heightMap.handle, m_heightMap.memory, 0);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkBindImageMemory failed with result=" << vkr << "\n";
        vkr = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        goto free_memory;
    }

    {
        VkImageViewCreateInfo viewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_heightMap.handle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R8_UNORM,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        vkr = vkCreateImageView(m_device, &viewCreateInfo, &g_allocCallbacks, &m_heightMap.view);
        if (vkr != VK_SUCCESS)
        {
            std::cout << "vkCreateImageView failed with result=" << vkr << "\n";
            vkr = VK_ERROR_INITIALIZATION_FAILED;
            goto free_memory;
        }
    }

    return vkr;

free_memory:
    vkFreeMemory(m_device, m_heightMap.memory, &g_allocCallbacks);
    m_heightMap.memory = VK_NULL_HANDLE;
destroy_image:
    vkDestroyImage(m_device, m_heightMap.handle, &g_allocCallbacks);
    m_heightMap.handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_create_sampler()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No device, cannot create sampler.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .compareEnable = VK_FALSE,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK
    };

    vkr = vkCreateSampler(m_device, &createInfo, &g_allocCallbacks, &m_sampler.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateSampler failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_descriptor_set_layout()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No device, cannot create descriptor set layout.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorSetLayoutBinding bindings[2] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT
        }
    };

    VkDescriptorSetLayoutCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };

    vkr = vkCreateDescriptorSetLayout(m_device, &createInfo, &g_allocCallbacks, &m_descriptorSetLayout.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDescriptorSetLayout failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_descriptor_pool()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No device, cannot create descriptor pool.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorPoolSize poolSizes[2] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
    };

    VkDescriptorPoolCreateInfo poolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes
    };

    vkr = vkCreateDescriptorPool(m_device, &poolCreateInfo, &g_allocCallbacks, &g_descriptorPool);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDescriptorPool failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_descriptor_set()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || g_descriptorPool == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No device or pool, cannot allocate descriptor set.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = g_descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descriptorSetLayout.handle
    };

    vkr = vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkAllocateDescriptorSets failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_camera_buffer(vk_buffer *outBuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || outBuffer == nullptr)
    {
        std::cout << "Warning: No device or output buffer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDeviceSize size = 64; // 4x4 matrix (16 floats = 64 bytes)

    VkBufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    vkr = vkCreateBuffer(m_device, &createInfo, &g_allocCallbacks, &outBuffer->handle);
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

    if (m_device == VK_NULL_HANDLE || buffer == nullptr)
    {
        std::cout << "Warning: No device or buffer.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, buffer->handle, &memRequirements);

    uint32_t memoryTypeIndex = find_memory_type(memRequirements.memoryTypeBits, properties);
    if (memoryTypeIndex == UINT32_MAX)
    {
        std::cout << "Warning: No suitable memory type.\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        goto destroy_buffer;
    }

    {
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memoryTypeIndex
        };

        vkr = vkAllocateMemory(m_device, &allocInfo, &g_allocCallbacks, &buffer->memory);
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
    vkFreeMemory(m_device, buffer->memory, &g_allocCallbacks);
    buffer->memory = VK_NULL_HANDLE;
destroy_buffer:
    vkDestroyBuffer(m_device, buffer->handle, &g_allocCallbacks);
    buffer->handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_create_framebuffer()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || m_renderPass.handle == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid device or render pass.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkFramebufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_renderPass.handle,
        .attachmentCount = 1,
        .pAttachments = &m_outputImage.view,
        .width = 256,
        .height = 256,
        .layers = 1
    };

    vkr = vkCreateFramebuffer(m_device, &createInfo, &g_allocCallbacks, &m_framebuffer.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateFramebuffer failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_displacement_pipeline()
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || m_shaderModules.size() < 3)
    {
        std::cout << "Warning: Invalid device or insufficient shader modules.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkPipelineShaderStageCreateInfo stages[3] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = m_shaderModules[0].handle,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            .module = m_shaderModules[1].handle,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            .module = m_shaderModules[2].handle,
            .pName = "main"
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineTessellationStateCreateInfo tessellationInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .patchControlPoints = 4
    };

    VkViewport viewport = {0.0f, 0.0f, 256.0f, 256.0f, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, {256, 256}};

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &m_descriptorSetLayout.handle,
        .pushConstantRangeCount = 0
    };

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vkr = vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, &g_allocCallbacks, &pipelineLayout);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreatePipelineLayout failed with result=" << vkr << "\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 3,
        .pStages = stages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState = &tessellationInfo,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .layout = pipelineLayout,
        .renderPass = m_renderPass.handle,
        .subpass = 0
    };

    vkr = vkCreateGraphicsPipeline(m_device, nullptr, 1, &pipelineInfo, &g_allocCallbacks, &m_pipeline.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateGraphicsPipeline failed with result=" << vkr << "\n";
        vkDestroyPipelineLayout(m_device, pipelineLayout, &g_allocCallbacks);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_cleanup()
{
    if (m_device != VK_NULL_HANDLE)
    {
        if (m_pipeline.handle != VK_NULL_HANDLE)
            vkDestroyPipeline(m_device, m_pipeline.handle, &g_allocCallbacks);

        if (m_renderPass.handle != VK_NULL_HANDLE)
            vkDestroyRenderPass(m_device, m_renderPass.handle, &g_allocCallbacks);

        if (m_framebuffer.handle != VK_NULL_HANDLE)
            vkDestroyFramebuffer(m_device, m_framebuffer.handle, &g_allocCallbacks);

        if (m_sampler.handle != VK_NULL_HANDLE)
            vkDestroySampler(m_device, m_sampler.handle, &g_allocCallbacks);

        if (m_descriptorSetLayout.handle != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout.handle, &g_allocCallbacks);

        if (g_descriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(m_device, g_descriptorPool, &g_allocCallbacks);

        if (m_outputImage.view != VK_NULL_HANDLE)
            vkDestroyImageView(m_device, m_outputImage.view, &g_allocCallbacks);

        if (m_outputImage.memory != VK_NULL_HANDLE)
            vkFreeMemory(m_device, m_outputImage.memory, &g_allocCallbacks);

        if (m_outputImage.handle != VK_NULL_HANDLE)
            vkDestroyImage(m_device, m_outputImage.handle, &g_allocCallbacks);

        if (m_heightMap.view != VK_NULL_HANDLE)
            vkDestroyImageView(m_device, m_heightMap.view, &g_allocCallbacks);

        if (m_heightMap.memory != VK_NULL_HANDLE)
            vkFreeMemory(m_device, m_heightMap.memory, &g_allocCallbacks);

        if (m_heightMap.handle != VK_NULL_HANDLE)
            vkDestroyImage(m_device, m_heightMap.handle, &g_allocCallbacks);

        for (const vk_buffer &buf : m_buffers)
        {
            vkDestroyBuffer(m_device, buf.handle, &g_allocCallbacks);
            vkFreeMemory(m_device, buf.memory, &g_allocCallbacks);
        }
        m_buffers.clear();

        for (const vk_shader_module &shader : m_shaderModules)
        {
            vkDestroyShaderModule(m_device, shader.handle, &g_allocCallbacks);
        }
        m_shaderModules.clear();

        vkDestroyDevice(m_device, &g_allocCallbacks);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, &g_allocCallbacks);
        m_instance = VK_NULL_HANDLE;
    }

    return VK_SUCCESS;
}

// ---- my_* functions ----

void my_init_vulkan()
{
    VkResult vkr = VK_INCOMPLETE;

    vkr = vk_init_instance();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Instance created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to initialize Vulkan instance.\n";
        return;
    default:
        std::cout << "Warning! vk_init_instance error=" << vkr << "\n";
        return;
    }

    vkr = vk_get_physical_device();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Physical device retrieved.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get physical device.\n";
        return;
    default:
        std::cout << "Warning! vk_get_physical_device error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_device();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Logical device created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create logical device.\n";
        return;
    default:
        std::cout << "Warning! vk_create_device error=" << vkr << "\n";
        return;
    }

    vkr = vk_get_queue();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Graphics queue obtained.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get graphics queue.\n";
        return;
    default:
        std::cout << "Warning! vk_get_queue error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_command_pool_and_buffer();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Command pool and buffer created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create command pool/buffer.\n";
        return;
    default:
        std::cout << "Warning! vk_create_command_pool_and_buffer error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_render_pass();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Render pass created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create render pass.\n";
        return;
    default:
        std::cout << "Warning! vk_create_render_pass error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_output_image();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Output image created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create output image.\n";
        return;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        std::cout << "Warning: Device memory exhausted.\n";
        return;
    default:
        std::cout << "Warning! vk_create_output_image error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_height_map_image();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Height map image created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create height map.\n";
        return;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        std::cout << "Warning: Device memory exhausted.\n";
        return;
    default:
        std::cout << "Warning! vk_create_height_map_image error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_sampler();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Sampler created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create sampler.\n";
        return;
    default:
        std::cout << "Warning! vk_create_sampler error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_descriptor_set_layout();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Descriptor set layout created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create descriptor set layout.\n";
        return;
    default:
        std::cout << "Warning! vk_create_descriptor_set_layout error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_descriptor_pool();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Descriptor pool created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create descriptor pool.\n";
        return;
    default:
        std::cout << "Warning! vk_create_descriptor_pool error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_descriptor_set();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Descriptor set allocated.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to allocate descriptor set.\n";
        return;
    default:
        std::cout << "Warning! vk_create_descriptor_set error=" << vkr << "\n";
        return;
    }

    vkr = vk_create_framebuffer();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Framebuffer created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create framebuffer.\n";
        return;
    default:
        std::cout << "Warning! vk_create_framebuffer error=" << vkr << "\n";
        return;
    }
}

void my_create_displacement_pipeline()
{
    VkResult vkr = vk_create_displacement_pipeline();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Displacement pipeline created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create displacement pipeline.\n";
        break;
    default:
        std::cout << "Warning! vk_create_displacement_pipeline error=" << vkr << "\n";
    }
}

void my_run_displacement()
{
    std::cout << "Displacement mapping demonstration (tessellation + shader sampling).\n";
}

// ---- dbg_* functions ----

void dbg_print_instance_extensions()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::cout << "Instance extensions: " << extensionCount << "\n";
}
