#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <iostream>
#include <fstream>
#include <vector>

VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkDevice m_device = VK_NULL_HANDLE;
VkQueue m_queue = VK_NULL_HANDLE;
uint32_t m_queueFamilyIndex = 0;

std::vector<vk_shader_module> m_shaderModules;
vk_render_pass m_renderPass = {};
vk_framebuffer m_framebuffer = {};
vk_pipeline m_pipeline = {};
vk_command_buffer m_commandBuffer = {};
vk_image m_outputImage = {};

static vk_allocator g_allocator;
static const VkAllocationCallbacks g_allocCallbacks = g_allocator;

size_t count_enabled_features(const VkPhysicalDeviceFeatures *features)
{
    const VkBool32 *p = (const VkBool32 *)features;
    size_t count = 0;
    for (size_t i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); ++i)
        if (p[i]) ++count;
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
            return i;
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

VkResult vk_init_instance()
{
    VkResult vkr = VK_INCOMPLETE;
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Geometry Shaders",
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
        std::cout << "Warning: No instance.\n";
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
        std::cout << "Warning: No physical device.\n";
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
        std::cout << "Warning: No device.\n";
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
        std::cout << "Warning: No device.\n";
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
        std::cout << "Warning: No device.\n";
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

VkResult vk_create_geometry_pipeline()
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
            .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
            .module = m_shaderModules[1].handle,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
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
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        .primitiveRestartEnable = VK_FALSE
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
        .setLayoutCount = 0,
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
        .pTessellationState = nullptr,
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

        if (m_outputImage.view != VK_NULL_HANDLE)
            vkDestroyImageView(m_device, m_outputImage.view, &g_allocCallbacks);

        if (m_outputImage.memory != VK_NULL_HANDLE)
            vkFreeMemory(m_device, m_outputImage.memory, &g_allocCallbacks);

        if (m_outputImage.handle != VK_NULL_HANDLE)
            vkDestroyImage(m_device, m_outputImage.handle, &g_allocCallbacks);

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

void my_init_vulkan()
{
    VkResult vkr;

    vkr = vk_init_instance();
    if (vkr != VK_SUCCESS) {
        std::cout << "Warning: Failed to initialize instance.\n";
        return;
    }

    vkr = vk_get_physical_device();
    if (vkr != VK_SUCCESS) {
        std::cout << "Warning: Failed to get physical device.\n";
        return;
    }

    vkr = vk_create_device();
    if (vkr != VK_SUCCESS) {
        std::cout << "Warning: Failed to create device.\n";
        return;
    }

    vkr = vk_get_queue();
    if (vkr != VK_SUCCESS) {
        std::cout << "Warning: Failed to get queue.\n";
        return;
    }

    vkr = vk_create_command_pool_and_buffer();
    if (vkr != VK_SUCCESS) {
        std::cout << "Warning: Failed to create command pool/buffer.\n";
        return;
    }

    vkr = vk_create_render_pass();
    if (vkr != VK_SUCCESS) {
        std::cout << "Warning: Failed to create render pass.\n";
        return;
    }

    vkr = vk_create_output_image();
    if (vkr != VK_SUCCESS) {
        std::cout << "Warning: Failed to create output image.\n";
        return;
    }

    vkr = vk_create_framebuffer();
    if (vkr != VK_SUCCESS) {
        std::cout << "Warning: Failed to create framebuffer.\n";
        return;
    }

    std::cout << "Vulkan initialization complete.\n";
}

void my_create_geometry_pipeline()
{
    VkResult vkr = vk_create_geometry_pipeline();
    if (vkr == VK_SUCCESS)
        std::cout << "Geometry pipeline created successfully.\n";
    else
        std::cout << "Warning: Failed to create geometry pipeline.\n";
}

void my_run_geometry()
{
    std::cout << "Geometry shader stage demonstration (point-to-billboard transformation).\n";
}
