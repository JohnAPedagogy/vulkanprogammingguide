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
VkQueue m_queue = VK_NULL_HANDLE;
uint32_t m_queueFamilyIndex = UINT32_MAX;
std::vector<vk_image> m_images;
std::vector<vk_command_buffer> m_commandBuffers;
VkCommandPool m_commandPool = VK_NULL_HANDLE;

// Host allocation callbacks for image/memory objects
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


// ---- vk_* setup functions ----

VkResult vk_init_instance(void)
{
    VkResult vkr = VK_INCOMPLETE;
    VkApplicationInfo appInfo = {};

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Ch04_03 Clear & Copy Images";
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

    vkr = vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateInstance failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_get_physical_device(void)
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
        std::cout << "vkEnumeratePhysicalDevices failed or found no devices, result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    VkPhysicalDevice devices[16];
    uint32_t queriedCount = std::min(deviceCount, (uint32_t)16);
    vkr = vkEnumeratePhysicalDevices(m_instance, &queriedCount, devices);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkEnumeratePhysicalDevices(query) failed, result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    m_physicalDevice = devices[0];
    return VK_SUCCESS;
}

VkResult vk_get_queue_family(void)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Physical device not selected.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0)
    {
        std::cout << "Warning: No queue families found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkQueueFamilyProperties queueFamilies[16];
    uint32_t queriedCount = std::min(queueFamilyCount, (uint32_t)16);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queriedCount, queueFamilies);

    m_queueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queriedCount; ++i)
    {
        if (queueFamilies[i].queueCount > 0 &&
            (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            m_queueFamilyIndex = i;
            break;
        }
    }

    if (m_queueFamilyIndex == UINT32_MAX)
    {
        std::cout << "Warning: No graphics queue family found.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkResult vk_create_logical_device(void)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_physicalDevice == VK_NULL_HANDLE || m_queueFamilyIndex == UINT32_MAX)
    {
        std::cout << "Warning: Physical device or queue family not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures = {};
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    vkr = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateDevice failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
        return vkr;
    }

    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
    return VK_SUCCESS;
}

VkResult vk_create_command_pool(void)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || m_queueFamilyIndex == UINT32_MAX)
    {
        std::cout << "Warning: Device not initialized or queue family not set.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    vkr = vkCreateCommandPool(m_device, &poolCreateInfo, &g_imageAllocCallbacks, &m_commandPool);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateCommandPool failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}

VkResult vk_create_command_buffer(vk_command_buffer *outCmdBuf)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE || m_commandPool == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Device or command pool not initialized.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vkr = vkAllocateCommandBuffers(m_device, &allocInfo, &outCmdBuf->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkAllocateCommandBuffers failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    return vkr;
}


// ---- vk_* resource creation ----

VkResult vk_create_image(uint32_t width, uint32_t height, VkFormat format,
                         VkImageUsageFlags usage, vk_image *outImage)
{
    VkResult vkr = VK_INCOMPLETE;

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Warning: No logical device, cannot create image.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = usage;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkr = vkCreateImage(m_device, &imageCreateInfo, &g_imageAllocCallbacks, &outImage->handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "vkCreateImage failed with result=" << vkr << "\n";
        vkr = VK_ERROR_INITIALIZATION_FAILED;
    }

    outImage->extent = imageCreateInfo.extent;
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
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

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


// ---- vk_* image operations - clear and copy ----

VkResult vk_record_clear_color_image(VkCommandBuffer cmdBuf, VkImage image,
                                     VkImageLayout layout,
                                     float r, float g, float b, float a)
{
    VkResult vkr = VK_INCOMPLETE;

    if (cmdBuf == VK_NULL_HANDLE || image == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid command buffer or image handle.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkClearColorValue clearValue = {};
    clearValue.float32[0] = r;
    clearValue.float32[1] = g;
    clearValue.float32[2] = b;
    clearValue.float32[3] = a;

    VkImageSubresourceRange range = {};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    vkCmdClearColorImage(cmdBuf, image, layout, &clearValue, 1, &range);

    vkr = VK_SUCCESS;
    return vkr;
}

VkResult vk_record_copy_image(VkCommandBuffer cmdBuf, VkImage srcImage,
                              VkImageLayout srcLayout, VkImage dstImage,
                              VkImageLayout dstLayout, uint32_t width, uint32_t height)
{
    VkResult vkr = VK_INCOMPLETE;

    if (cmdBuf == VK_NULL_HANDLE || srcImage == VK_NULL_HANDLE || dstImage == VK_NULL_HANDLE)
    {
        std::cout << "Warning: Invalid command buffer or image handle.\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkImageSubresourceLayers subresource = {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = 0;
    subresource.baseArrayLayer = 0;
    subresource.layerCount = 1;

    VkImageCopy region = {};
    region.srcSubresource = subresource;
    region.dstSubresource = subresource;
    region.srcOffset = {0, 0, 0};
    region.dstOffset = {0, 0, 0};
    region.extent = {width, height, 1};

    vkCmdCopyImage(cmdBuf, srcImage, srcLayout, dstImage, dstLayout, 1, &region);

    vkr = VK_SUCCESS;
    return vkr;
}


// ---- vk_* cleanup ----

VkResult vk_cleanup(void)
{
    for (const vk_image &img : m_images)
    {
        vkDestroyImage(m_device, img.handle, &g_imageAllocCallbacks);
        vkFreeMemory(m_device, img.memory, &g_imageAllocCallbacks);
    }
    m_images.clear();

    for (const vk_command_buffer &cmdBuf : m_commandBuffers)
    {
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmdBuf.handle);
    }
    m_commandBuffers.clear();

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, &g_imageAllocCallbacks);
        m_commandPool = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    return VK_SUCCESS;
}


// ---- my_* orchestration functions ----

void my_init_vulkan(void)
{
#ifdef ENABLE_VALIDATION
    if (!getenv("VK_LAYER_PATH"))
    {
        _putenv_s("VK_LAYER_PATH", VK_LAYER_PATH);
        std::cout << "VK_LAYER_PATH = " << VK_LAYER_PATH << "\n";
    }
#endif

    VkResult vkr = vk_init_instance();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Instance created successfully.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create instance.\n";
        return;
    default:
        std::cout << "Warning! vk_init_instance error=" << vkr << "\n";
        return;
    }

    vkr = vk_get_physical_device();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Physical device selected.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get physical device.\n";
        vk_cleanup();
        return;
    default:
        std::cout << "Warning! vk_get_physical_device error=" << vkr << "\n";
        vk_cleanup();
        return;
    }

    vkr = vk_get_queue_family();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Queue family selected.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to get queue family.\n";
        vk_cleanup();
        return;
    default:
        std::cout << "Warning! vk_get_queue_family error=" << vkr << "\n";
        vk_cleanup();
        return;
    }

    vkr = vk_create_logical_device();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Logical device created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create logical device.\n";
        vk_cleanup();
        return;
    default:
        std::cout << "Warning! vk_create_logical_device error=" << vkr << "\n";
        vk_cleanup();
        return;
    }

    vkr = vk_create_command_pool();
    switch (vkr)
    {
    case VK_SUCCESS:
        std::cout << "Command pool created.\n";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        std::cout << "Warning: Failed to create command pool.\n";
        vk_cleanup();
        return;
    default:
        std::cout << "Warning! vk_create_command_pool error=" << vkr << "\n";
        vk_cleanup();
        return;
    }
}

void my_clear_and_copy_image(void)
{
    // Create source image for clearing
    vk_image srcImg;
    VkResult vkr = vk_create_image(256, 256, VK_FORMAT_R8G8B8A8_SRGB,
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                    &srcImg);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Source image creation failed.\n";
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Image object could not be created.\n";
            break;
        default:
            std::cout << "vk_create_image error=" << vkr << "\n";
        }
        return;
    }

    vkr = vk_track_image(&srcImg, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Source image memory tracking failed.\n";
        switch (vkr)
        {
        case VK_ERROR_INITIALIZATION_FAILED:
            std::cout << "Image memory allocation failed.\n";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            std::cout << "Not enough device memory for image.\n";
            break;
        default:
            std::cout << "vk_track_image error=" << vkr << "\n";
        }
        return;
    }

    std::cout << "Source image created: " << srcImg.extent.width << "x" << srcImg.extent.height << "\n";

    // Create destination image
    vk_image dstImg;
    vkr = vk_create_image(256, 256, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                          &dstImg);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Destination image creation failed.\n";
        return;
    }

    vkr = vk_track_image(&dstImg, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Destination image memory tracking failed.\n";
        return;
    }

    std::cout << "Destination image created: " << dstImg.extent.width << "x" << dstImg.extent.height << "\n";

    // Create command buffer
    vk_command_buffer cmdBuf;
    vkr = vk_create_command_buffer(&cmdBuf);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: Command buffer creation failed.\n";
        return;
    }

    m_commandBuffers.push_back(cmdBuf);

    // Begin recording
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkr = vkBeginCommandBuffer(cmdBuf.handle, &beginInfo);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: vkBeginCommandBuffer failed with result=" << vkr << "\n";
        return;
    }

    // Clear the source image
    vkr = vk_record_clear_color_image(cmdBuf.handle, srcImg.handle,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      0.1f, 0.2f, 0.3f, 1.0f);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Clear command recorded: color=(0.1, 0.2, 0.3, 1.0)\n";
    }
    else
    {
        std::cout << "Warning: vk_record_clear_color_image failed with result=" << vkr << "\n";
    }

    // Copy from source to destination
    vkr = vk_record_copy_image(cmdBuf.handle, srcImg.handle,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               dstImg.handle,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               256, 256);
    if (vkr == VK_SUCCESS)
    {
        std::cout << "Copy command recorded: 256x256 pixels\n";
    }
    else
    {
        std::cout << "Warning: vk_record_copy_image failed with result=" << vkr << "\n";
    }

    vkr = vkEndCommandBuffer(cmdBuf.handle);
    if (vkr != VK_SUCCESS)
    {
        std::cout << "Warning: vkEndCommandBuffer failed with result=" << vkr << "\n";
    }
    else
    {
        std::cout << "Command buffer recording completed.\n";
    }
}
