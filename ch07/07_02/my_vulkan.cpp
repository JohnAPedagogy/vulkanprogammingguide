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
std::vector<vk_image> m_images;
std::vector<vk_image_view> m_imageViews;
std::vector<vk_render_pass> m_renderPasses;
std::vector<vk_framebuffer> m_framebuffers;
int device_count = 0;

// Host allocation callbacks
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

    std::cout << "vkCreateInstance called\n";
    result = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result" << result << "\n";
        return VK_NOT_READY;
    }
    std::cout << "vkCreateInstance succeeded\n";

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
        std::cout << "Warning Graphics device not present.\n";
        return VK_NOT_READY;
    }

    VkQueueFamilyProperties* queueFamilyProperties = nullptr;
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_devices[deviceIndex],
        queueFamilyPropertyCount,
        nullptr);
    if(*queueFamilyPropertyCount == 0)
    {
        std::cout << "Warning Device family not found bailing...\n";
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
    for (const vk_framebuffer &fb : m_framebuffers)
    {
        vkDestroyFramebuffer(m_device, fb.handle, &g_imageAllocCallbacks);
    }
    m_framebuffers.clear();

    for (const vk_image_view &iv : m_imageViews)
    {
        vkDestroyImageView(m_device, iv.handle, &g_imageAllocCallbacks);
    }
    m_imageViews.clear();

    for (const vk_image &img : m_images)
    {
        vkDestroyImage(m_device, img.handle, &g_imageAllocCallbacks);
        vkFreeMemory(m_device, img.memory, &g_imageAllocCallbacks);
    }
    m_images.clear();

    for (const vk_render_pass &rp : m_renderPasses)
    {
        vkDestroyRenderPass(m_device, rp.handle, &g_imageAllocCallbacks);
    }
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
            std::cout << "Attempting device creation without mandatory tessellation/geometry features...\n";
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
    return VK_SUCCESS;
}


VkResult vk_get_layer_properties(int device_index, uint32_t *numInstanceLayers)
{
    VkResult vkr = VK_INCOMPLETE;
    VkLayerProperties* instanceLayerProperties = nullptr;
    VkLayerProperties* deviceLayerProperties = nullptr;

    if (numInstanceLayers == nullptr)
        return VK_ERROR_INITIALIZATION_FAILED;

    vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                       nullptr);

    if (*numInstanceLayers != 0)
    {
        instanceLayerProperties = (VkLayerProperties*)malloc(*numInstanceLayers * sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                           instanceLayerProperties);
        vkr = VK_SUCCESS;
    }
    else
    {
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    uint32_t deviceLayerCnt;
    vkEnumerateDeviceLayerProperties(m_devices[device_index],
                                     &deviceLayerCnt,
                                       nullptr);

    if (deviceLayerCnt != 0)
    {
        deviceLayerProperties = (VkLayerProperties*)malloc(deviceLayerCnt * sizeof(VkLayerProperties));
        vkEnumerateDeviceLayerProperties(m_devices[device_index],
                                         &deviceLayerCnt,
                                           deviceLayerProperties);
        vkr = VK_SUCCESS;
    }
    else
    {
        return VK_ERROR_LAYER_NOT_PRESENT;
    }
    std::cout << "Showing "<< *numInstanceLayers <<" Instance layer Properties***\n";
    dbg_show_layer_property_names(instanceLayerProperties, *numInstanceLayers);
    std::cout << "\nShowing "<< deviceLayerCnt  << " Device layer Properties***\n";
    dbg_show_layer_property_names(deviceLayerProperties, deviceLayerCnt);
    free(deviceLayerProperties);
    free(instanceLayerProperties);
    return vkr;
}


VkResult vk_get_extensions(uint32_t* numInstanceExtensions)
{
    VkResult vkr = VK_INCOMPLETE;
    std::vector<VkExtensionProperties> instanceExtensionProperties;

    vkEnumerateInstanceExtensionProperties(nullptr,
                                           numInstanceExtensions,
                                           nullptr);

    if (*numInstanceExtensions == 0)
    {
        std::cout << "Warning: No instance extensions found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    instanceExtensionProperties.resize(*numInstanceExtensions);
    vkr = vkEnumerateInstanceExtensionProperties(nullptr,
                                                 numInstanceExtensions,
                                                 instanceExtensionProperties.data());
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumerateInstanceExtensionProperties failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}


VkResult vk_create_render_pass(VkFormat colorFormat, VkRenderPass *outRenderPass)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create render pass.\n";
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
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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
        0, nullptr,
        1, &colorAttachmentRef,
        nullptr,
        nullptr,
        0, nullptr
    };

    const VkRenderPassCreateInfo renderPassInfo =
    {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        nullptr,
        0,
        1, &colorAttachment,
        1, &subpass,
        0, nullptr
    };

    vkr = vkCreateRenderPass(m_device, &renderPassInfo, &g_imageAllocCallbacks, outRenderPass);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateRenderPass failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}


VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, vk_image *outImage)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create image.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkImageCreateInfo imageCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        nullptr,
        0,
        VK_IMAGE_TYPE_2D,
        format,
        { width, height, 1 },
        1,
        1,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        VK_SHARING_MODE_EXCLUSIVE,
        0, nullptr,
        VK_IMAGE_LAYOUT_UNDEFINED
    };

    vkr = vkCreateImage(m_device, &imageCreateInfo, &g_imageAllocCallbacks, &outImage->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImage failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

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

    m_images.push_back(*image);
    return vkr;

free_memory:
    vkFreeMemory(m_device, image->memory, &g_imageAllocCallbacks);
    image->memory = VK_NULL_HANDLE;
destroy_image:
    vkDestroyImage(m_device, image->handle, &g_imageAllocCallbacks);
    image->handle = VK_NULL_HANDLE;
    return vkr;
}


VkResult vk_create_image_view(VkImage imageHandle, VkFormat format, VkImageView *outImageView)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create image view.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkImageViewCreateInfo viewCreateInfo =
    {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0,
        imageHandle,
        VK_IMAGE_VIEW_TYPE_2D,
        format,
        { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkr = vkCreateImageView(m_device, &viewCreateInfo, &g_imageAllocCallbacks, outImageView);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImageView failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}


VkResult vk_create_framebuffer(VkRenderPass renderPass, VkImageView colorView, uint32_t width, uint32_t height, VkFramebuffer *outFramebuffer)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE || colorView == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid parameters for framebuffer creation.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const VkFramebufferCreateInfo framebufferInfo =
    {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        nullptr,
        0,
        renderPass,
        1, &colorView,
        width, height, 1
    };

    vkr = vkCreateFramebuffer(m_device, &framebufferInfo, &g_imageAllocCallbacks, outFramebuffer);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateFramebuffer failed with result=" << vkr << "\n";
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

        std::cout << "--- vulkaninfo validation layer check ---\n";
#ifdef _WIN32
        std::string cmd = std::string(VK_LAYER_PATH) + "/vulkaninfo.exe --summary 2>&1";
#else
        std::string cmd = "vulkaninfo --summary 2>&1";
#endif
        FILE *pipe = popen(cmd.c_str(), "r");
        if (pipe)
        {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe))
            {
                std::string line(buf);
                std::string lower = line;
                for (auto &c : lower) c = (char)tolower((unsigned char)c);
                if (lower.find("valid") != std::string::npos)
                    std::cout << line;
            }
            pclose(pipe);
        }
        std::cout << "--- end validation check ---\n";
    }
#endif

    std::cout << "Checking for physical graphics devices..\n";
    int rc = vk_device_init_count(&device_count);
    if(rc == VK_SUCCESS) {
        std::cout << "Found " << device_count << " physical graphics devices.\n";
    }else {
        switch(rc){
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Initialisation failed.\n";
            break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            std::cout << "Out of host memory.\n";
            break;
        case VK_NOT_READY:
            std::cout << "No instance found.\n";
            break;
        default:
            std::cout << "Unkown error code " << rc << "\n";
        }
    }
    std::cout << "my_init_vulkan completed with  " << rc << "\n";
}

void my_get_device_properties(int device_index)
{
    uint32_t dev_prop_count = 0;
    VkResult vkr = vk_get_device_properties(device_index, &dev_prop_count);
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << dev_prop_count << " properties found for device[" << device_index << "]\n";
        break;
    case VK_NOT_READY:
        std::cout << "Warning: Graphics device not present.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Device queue family not found.\n";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        std::cout << "Warning: Out of host memory retrieving device properties.\n";
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
        std::cout << features << " features present on device[" << device_index << "]\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Invalid device index or no device available.\n";
        break;
    case VK_ERROR_FEATURE_NOT_PRESENT:
        std::cout << "Warning: Requested graphics feature(s) not supported.\n";
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
        std::cout << layer_count << " layers found!\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Invalid layer count pointer.\n";
        break;
    case VK_ERROR_LAYER_NOT_PRESENT:
        std::cout << "Warning: No instance or device layers present.\n";
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
        std::cout << count << " extensions found!\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Could not retrieve instance extensions.\n";
        break;
    default:
        std::cout << "Warning! vk_get_extensions error=" << vkr << "\n";
    }
}

void my_create_framebuffer_setup(void)
{
    const uint32_t width = 256;
    const uint32_t height = 256;
    VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult vkr = vk_create_render_pass(format, &renderPass);
    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Render pass could not be created.\n";
            break;
        default:
            std::cout << "Warning! vk_create_render_pass error=" << vkr << "\n";
        }
        return;
    }

    vk_render_pass rp;
    rp.handle = renderPass;
    m_renderPasses.push_back(rp);

    vk_image img;
    vkr = vk_create_image(width, height, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &img);
    if (vkr == VK_SUCCESS)
    {
        vkr = vk_track_image(&img, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Image could not be created.\n";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cout << "Warning: Not enough device memory for image.\n";
            break;
        default:
            std::cout << "Warning! vk_create_image error=" << vkr << "\n";
        }
        return;
    }

    vk_image_view imgView;
    vkr = vk_create_image_view(img.handle, format, &imgView.handle);
    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Image view could not be created.\n";
            break;
        default:
            std::cout << "Warning! vk_create_image_view error=" << vkr << "\n";
        }
        return;
    }

    m_imageViews.push_back(imgView);

    vk_framebuffer fb;
    vkr = vk_create_framebuffer(renderPass, imgView.handle, width, height, &fb.handle);
    if (vkr != VK_SUCCESS)
    {
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Warning: Framebuffer could not be created.\n";
            break;
        default:
            std::cout << "Warning! vk_create_framebuffer error=" << vkr << "\n";
        }
        return;
    }

    m_framebuffers.push_back(fb);
    std::cout << "Framebuffer and attachments created!\n";
}


// ---- dbg_* functions ----

void dbg_show_layer_property_names(VkLayerProperties* p, int count)
{
    for(int i = 0; i < count; i++)
        std::cout << p[i].layerName << "\n";
}
