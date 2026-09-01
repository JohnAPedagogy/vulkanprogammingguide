#include "my_vulkan.h"
#include "../../ch02/0201_allocator.h"
#include <stddef.h>
#include <iostream>
#include <stdlib.h>
#include <cstdio>
#include <string>
#include <fstream>
#include <vector>


VkInstance m_instance = VK_NULL_HANDLE;
VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice *m_devices = nullptr;
VkDevice m_device = VK_NULL_HANDLE;
std::vector<vk_image> m_images;
std::vector<vk_image_view> m_imageViews;
std::vector<vk_render_pass> m_renderPasses;
std::vector<vk_framebuffer> m_framebuffers;
std::vector<vk_shader_module> m_shaderModules;
std::vector<vk_pipeline_layout> m_pipelineLayouts;
std::vector<vk_pipeline> m_pipelines;
int device_count = 0;

static vk_allocator g_pipelineAllocator;
static const VkAllocationCallbacks g_pipelineAllocCallbacks = g_pipelineAllocator;


// ---- helper functions ----

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
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    std::cout << "Warning: No suitable memory type found.\n";
    return UINT32_MAX;
}

static std::vector<char> read_shader_file(const char *path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "Warning: Failed to open shader file: " << path << "\n";
        return std::vector<char>();
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
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
    appInfo.pApplicationName = "Application";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result" << result << "\n";
        return VK_NOT_READY;
    }

    uint32_t physicalDevCount = 0;
    result = vkEnumeratePhysicalDevices(m_instance, &physicalDevCount, nullptr);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices failed with result " << result << "\n";
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
    if(queueFamilyPropertyCount == nullptr || deviceIndex < 0 || device_count <= deviceIndex || m_devices == nullptr)
        return VK_NOT_READY;

    VkQueueFamilyProperties* queueFamilyProperties = nullptr;
    vkGetPhysicalDeviceQueueFamilyProperties(m_devices[deviceIndex], queueFamilyPropertyCount, nullptr);
    if(*queueFamilyPropertyCount == 0)
        return VK_ERROR_INITIALIZATION_FAILED;

    queueFamilyProperties =(VkQueueFamilyProperties*)malloc(*queueFamilyPropertyCount * sizeof(VkQueueFamilyProperties));
    if (queueFamilyProperties == nullptr)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    vkGetPhysicalDeviceQueueFamilyProperties(m_devices[deviceIndex], queueFamilyPropertyCount, queueFamilyProperties);
    vkr = VK_SUCCESS;
    free(queueFamilyProperties);
    return vkr;
}

VkResult vk_cleanup()
{
    for (const vk_pipeline &p : m_pipelines)
        vkDestroyPipeline(m_device, p.handle, &g_pipelineAllocCallbacks);
    m_pipelines.clear();

    for (const vk_pipeline_layout &pl : m_pipelineLayouts)
        vkDestroyPipelineLayout(m_device, pl.handle, &g_pipelineAllocCallbacks);
    m_pipelineLayouts.clear();

    for (const vk_shader_module &sm : m_shaderModules)
        vkDestroyShaderModule(m_device, sm.handle, &g_pipelineAllocCallbacks);
    m_shaderModules.clear();

    for (const vk_framebuffer &fb : m_framebuffers)
        vkDestroyFramebuffer(m_device, fb.handle, &g_pipelineAllocCallbacks);
    m_framebuffers.clear();

    for (const vk_image_view &iv : m_imageViews)
        vkDestroyImageView(m_device, iv.handle, &g_pipelineAllocCallbacks);
    m_imageViews.clear();

    for (const vk_image &img : m_images)
    {
        vkDestroyImage(m_device, img.handle, &g_pipelineAllocCallbacks);
        vkFreeMemory(m_device, img.memory, &g_pipelineAllocCallbacks);
    }
    m_images.clear();

    for (const vk_render_pass &rp : m_renderPasses)
        vkDestroyRenderPass(m_device, rp.handle, &g_pipelineAllocCallbacks);
    m_renderPasses.clear();

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
        return VK_ERROR_INITIALIZATION_FAILED;

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
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, 0, 1, &queuePriority
    };

    *feature_count = (int)count_enabled_features(&supportedFeatures);
    const VkDeviceCreateInfo deviceCreateInfo =
    {
        VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0, 1, &deviceQueueCreateInfo,
        0, nullptr, 0, nullptr, &requiredFeatures
    };

    if (m_devices[device_index] == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    result = vkCreateDevice(m_devices[device_index], &deviceCreateInfo, nullptr, &m_device);

    if (result != VK_SUCCESS)
    {
        if (result == VK_ERROR_FEATURE_NOT_PRESENT)
        {
            requiredFeatures.tessellationShader = VK_FALSE;
            requiredFeatures.geometryShader = VK_FALSE;
            result = vkCreateDevice(m_devices[device_index], &deviceCreateInfo, nullptr, &m_device);
        }
        if (result != VK_SUCCESS)
            return VK_ERROR_FEATURE_NOT_PRESENT;
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

    vkEnumerateInstanceLayerProperties(numInstanceLayers, nullptr);

    if (*numInstanceLayers != 0)
    {
        instanceLayerProperties = (VkLayerProperties*)malloc(*numInstanceLayers * sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(numInstanceLayers, instanceLayerProperties);
        vkr = VK_SUCCESS;
    }
    else
        return VK_ERROR_LAYER_NOT_PRESENT;

    uint32_t deviceLayerCnt;
    vkEnumerateDeviceLayerProperties(m_devices[device_index], &deviceLayerCnt, nullptr);

    if (deviceLayerCnt != 0)
    {
        deviceLayerProperties = (VkLayerProperties*)malloc(deviceLayerCnt * sizeof(VkLayerProperties));
        vkEnumerateDeviceLayerProperties(m_devices[device_index], &deviceLayerCnt, deviceLayerProperties);
        vkr = VK_SUCCESS;
    }
    else
        return VK_ERROR_LAYER_NOT_PRESENT;

    free(deviceLayerProperties);
    free(instanceLayerProperties);
    return vkr;
}

VkResult vk_get_extensions(uint32_t* numInstanceExtensions)
{
    VkResult vkr = VK_INCOMPLETE;
    std::vector<VkExtensionProperties> instanceExtensionProperties;

    vkEnumerateInstanceExtensionProperties(nullptr, numInstanceExtensions, nullptr);

    if (*numInstanceExtensions == 0)
        return VK_ERROR_INITIALIZATION_FAILED;

    instanceExtensionProperties.resize(*numInstanceExtensions);
    vkr = vkEnumerateInstanceExtensionProperties(nullptr, numInstanceExtensions, instanceExtensionProperties.data());
    if (vkr != VK_SUCCESS)
        vkr = VK_ERROR_INITIALIZATION_FAILED;

    return vkr;
}

VkResult vk_create_render_pass(VkFormat colorFormat, VkRenderPass *outRenderPass)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkAttachmentDescription colorAttachment =
    {
        0, colorFormat, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    const VkAttachmentReference colorAttachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    const VkSubpassDescription subpass =
    {
        0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, &colorAttachmentRef, nullptr, nullptr, 0, nullptr
    };

    const VkRenderPassCreateInfo renderPassInfo =
    {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 1, &colorAttachment, 1, &subpass, 0, nullptr
    };

    vkr = vkCreateRenderPass(m_device, &renderPassInfo, &g_pipelineAllocCallbacks, outRenderPass);
    if (vkr != VK_SUCCESS)
        vkr = VK_ERROR_INITIALIZATION_FAILED;

    return vkr;
}

VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, vk_image *outImage)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkImageCreateInfo imageCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0, VK_IMAGE_TYPE_2D, format, { width, height, 1 },
        1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };

    vkr = vkCreateImage(m_device, &imageCreateInfo, &g_pipelineAllocCallbacks, &outImage->handle);
    if (vkr != VK_SUCCESS)
        vkr = VK_ERROR_INITIALIZATION_FAILED;

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
        const VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memRequirements.size, memoryTypeIndex };
        vkr = vkAllocateMemory(m_device, &allocInfo, &g_pipelineAllocCallbacks, &image->memory);
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

    m_images.push_back(*image);
    return vkr;

free_memory:
    vkFreeMemory(m_device, image->memory, &g_pipelineAllocCallbacks);
    image->memory = VK_NULL_HANDLE;
destroy_image:
    vkDestroyImage(m_device, image->handle, &g_pipelineAllocCallbacks);
    image->handle = VK_NULL_HANDLE;
    return vkr;
}

VkResult vk_create_image_view(VkImage imageHandle, VkFormat format, VkImageView *outImageView)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkImageViewCreateInfo viewCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, imageHandle, VK_IMAGE_VIEW_TYPE_2D, format,
        { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkr = vkCreateImageView(m_device, &viewCreateInfo, &g_pipelineAllocCallbacks, outImageView);
    if (vkr != VK_SUCCESS)
        vkr = VK_ERROR_INITIALIZATION_FAILED;

    return vkr;
}

VkResult vk_create_framebuffer(VkRenderPass renderPass, VkImageView colorView, uint32_t width, uint32_t height, VkFramebuffer *outFramebuffer)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || colorView == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkFramebufferCreateInfo framebufferInfo =
    {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, renderPass, 1, &colorView, width, height, 1
    };

    vkr = vkCreateFramebuffer(m_device, &framebufferInfo, &g_pipelineAllocCallbacks, outFramebuffer);
    if (vkr != VK_SUCCESS)
        vkr = VK_ERROR_INITIALIZATION_FAILED;

    return vkr;
}

VkResult vk_create_shader_module(const char *spvPath, VkShaderModule *outModule)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE || spvPath == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;

    std::vector<char> code = read_shader_file(spvPath);
    if (code.empty())
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkShaderModuleCreateInfo createInfo =
    {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        nullptr,
        0,
        code.size(),
        (const uint32_t*)code.data()
    };

    vkr = vkCreateShaderModule(m_device, &createInfo, &g_pipelineAllocCallbacks, outModule);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateShaderModule failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_pipeline_layout(VkPipelineLayout *outLayout)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkPipelineLayoutCreateInfo layoutInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        nullptr,
        0,
        0, nullptr,
        0, nullptr
    };

    vkr = vkCreatePipelineLayout(m_device, &layoutInfo, &g_pipelineAllocCallbacks, outLayout);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreatePipelineLayout failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_graphics_pipeline(VkRenderPass renderPass, VkPipelineLayout layout, VkShaderModule vertShader, VkShaderModule fragShader, VkPipeline *outPipeline)
{
    VkResult vkr = VK_INCOMPLETE;
    if (m_device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || layout == VK_NULL_HANDLE)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkPipelineShaderStageCreateInfo shaderStages[] =
    {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
            VK_SHADER_STAGE_VERTEX_BIT, vertShader, "main", nullptr
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
            VK_SHADER_STAGE_FRAGMENT_BIT, fragShader, "main", nullptr
        }
    };

    const VkPipelineVertexInputStateCreateInfo vertexInputInfo =
    {
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr, 0, 0, nullptr, 0, nullptr
    };

    const VkPipelineInputAssemblyStateCreateInfo inputAssembly =
    {
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE
    };

    const VkViewport viewport = { 0.0f, 0.0f, 256.0f, 256.0f, 0.0f, 1.0f };
    const VkRect2D scissor = { {0, 0}, {256, 256} };

    const VkPipelineViewportStateCreateInfo viewportState =
    {
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, &viewport, 1, &scissor
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer =
    {
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE,
        VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f
    };

    const VkPipelineMultisampleStateCreateInfo multisampling =
    {
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT,
        VK_FALSE, 1.0f, nullptr, VK_FALSE, VK_FALSE
    };

    const VkPipelineDepthStencilStateCreateInfo depthStencil =
    {
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, nullptr, 0,
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE,
        {}, {}, 0.0f, 1.0f
    };

    const VkPipelineColorBlendAttachmentState colorBlendAttachment =
    {
        VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    const VkPipelineColorBlendStateCreateInfo colorBlending =
    {
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY,
        1, &colorBlendAttachment, {0.0f, 0.0f, 0.0f, 0.0f}
    };

    const VkGraphicsPipelineCreateInfo pipelineInfo =
    {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0,
        2, shaderStages,
        &vertexInputInfo, &inputAssembly, nullptr, &viewportState, &rasterizer, &multisampling,
        &depthStencil, &colorBlending, nullptr,
        layout, renderPass, 0, VK_NULL_HANDLE, -1
    };

    vkr = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, &g_pipelineAllocCallbacks, outPipeline);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateGraphicsPipelines failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
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
    int rc = vk_device_init_count(&device_count);
    std::cout << "Found " << device_count << " graphics devices.\n";
}

void my_get_device_properties(int device_index)
{
    uint32_t dev_prop_count = 0;
    VkResult vkr = vk_get_device_properties(device_index, &dev_prop_count);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << dev_prop_count << " properties found.\n";
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
        std::cout << features << " features present.\n";
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
        std::cout << layer_count << " layers found.\n";
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
        std::cout << count << " extensions found.\n";
        break;
    default:
        std::cout << "Warning! vk_get_extensions error=" << vkr << "\n";
    }
}

void my_create_graphics_pipeline(void)
{
    const uint32_t width = 256;
    const uint32_t height = 256;
    VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult vkr = vk_create_render_pass(format, &renderPass);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create render pass.\n";
        return;
    }
    vk_render_pass rp;
    rp.handle = renderPass;
    m_renderPasses.push_back(rp);

    vk_image img;
    vkr = vk_create_image(width, height, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &img);
    if (vkr == VK_SUCCESS)
        vkr = vk_track_image(&img, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create image.\n";
        return;
    }

    vk_image_view imgView;
    vkr = vk_create_image_view(img.handle, format, &imgView.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create image view.\n";
        return;
    }
    m_imageViews.push_back(imgView);

    vk_framebuffer fb;
    vkr = vk_create_framebuffer(renderPass, imgView.handle, width, height, &fb.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create framebuffer.\n";
        return;
    }
    m_framebuffers.push_back(fb);

    VkShaderModule vertShader = VK_NULL_HANDLE;
    VkShaderModule fragShader = VK_NULL_HANDLE;

    vkr = vk_create_shader_module("shaders/triangle.vert.spv", &vertShader);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create vertex shader.\n";
        return;
    }
    vk_shader_module sm;
    sm.handle = vertShader;
    m_shaderModules.push_back(sm);

    vkr = vk_create_shader_module("shaders/triangle.frag.spv", &fragShader);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create fragment shader.\n";
        return;
    }
    sm.handle = fragShader;
    m_shaderModules.push_back(sm);

    VkPipelineLayout layout = VK_NULL_HANDLE;
    vkr = vk_create_pipeline_layout(&layout);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create pipeline layout.\n";
        return;
    }
    vk_pipeline_layout pl;
    pl.handle = layout;
    m_pipelineLayouts.push_back(pl);

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkr = vk_create_graphics_pipeline(renderPass, layout, vertShader, fragShader, &pipeline);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Failed to create graphics pipeline.\n";
        return;
    }
    vk_pipeline p;
    p.handle = pipeline;
    m_pipelines.push_back(p);

    std::cout << "Graphics pipeline created!\n";
}

void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    for(int i = 0; i < count; i++)
        std::cout << p[i].layerName << "\n";
}
