#include "my_vulkan.h"
#include <iostream>

int main()
{
    std::cout << "=== Vulkan Tutorial 5.3: Acquire and Present Loop ===\n\n";

    // Initialize Vulkan instance with GLFW extensions
    my_init_vulkan();

    if (device_count <= 0)
    {
        std::cout << "No graphics devices found!\n";
        return 1;
    }

    // Create a GLFW window
    my_create_window();

    if (m_window == nullptr)
    {
        std::cout << "Failed to create window. Cleaning up.\n";
        vk_cleanup();
        return 1;
    }

    // Query device properties before creating logical device
    int active_device_index = 0;
    my_get_device_properties(active_device_index);

    // Create logical device
    my_get_logical_device(active_device_index);

    if (m_device == VK_NULL_HANDLE)
    {
        std::cout << "Failed to create logical device. Cleaning up.\n";
        vk_cleanup();
        return 1;
    }

    // Create a Vulkan surface from the GLFW window
    my_create_surface();

    if (m_surface == VK_NULL_HANDLE)
    {
        std::cout << "Failed to create surface. Cleaning up.\n";
        vk_cleanup();
        return 1;
    }

    // Check if the device supports presentation to this surface
    my_check_present_support(active_device_index);

    // Create a swap chain
    my_create_swapchain(800, 600);

    if (m_swapchain == VK_NULL_HANDLE)
    {
        std::cout << "Failed to create swap chain. Cleaning up.\n";
        vk_cleanup();
        return 1;
    }

    // Get the swap chain images
    my_get_swapchain_images();

    // Get the present queue
    my_get_present_queue();

    if (m_presentQueue == VK_NULL_HANDLE)
    {
        std::cout << "Failed to get present queue. Cleaning up.\n";
        vk_cleanup();
        return 1;
    }

    // Create frame synchronization objects (1 frame in flight for now)
    my_create_frame_sync_objects((uint32_t)m_swapchainImages.size());

    std::cout << "\n=== Starting acquire/present loop ===\n";
    std::cout << "Window will close after 100 frames or manual close.\n\n";

    // Frame loop (§12: acquire → render → present shape)
    uint32_t frameCount = 0;
    const uint32_t maxFrames = 100;

    while (!glfwWindowShouldClose(m_window) && frameCount < maxFrames)
    {
        glfwPollEvents();

        if (m_swapchainImages.empty() || m_imageAvailableSemaphores.empty())
        {
            std::cout << "Warning: Swap chain or sync objects not properly initialized.\n";
            break;
        }

        uint32_t frameIndex = frameCount % m_swapchainImages.size();

        // Acquire next image
        uint32_t imageIndex;
        VkResult vkr = vkAcquireNextImageKHR(
            m_device,
            m_swapchain,
            UINT64_MAX,
            m_imageAvailableSemaphores[frameIndex],
            VK_NULL_HANDLE,
            &imageIndex);

        if (vkr != VK_SUCCESS)
        {
            std::cout << "Warning: vkAcquireNextImageKHR failed with result=" << vkr << "\n";

            // Handle out-of-date swapchain (note: in real applications, would rebuild)
            if (vkr == VK_ERROR_OUT_OF_DATE_KHR || vkr == VK_SUBOPTIMAL_KHR)
            {
                std::cout << "Note: Swapchain is suboptimal or out of date.\n";
                break;
            }
            else
            {
                break;
            }
        }

        std::cout << "Frame " << frameCount << ": Acquired image " << imageIndex << "\n";

        // Present the image (no render commands in this basic demo)
        const VkPresentInfoKHR presentInfo = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, // sType
            nullptr,                             // pNext
            1,                                   // waitSemaphoreCount
            &m_renderFinishedSemaphores[frameIndex], // pWaitSemaphores
            1,                                   // swapchainCount
            &m_swapchain,                        // pSwapchains
            &imageIndex,                         // pImageIndices
            nullptr                              // pResults
        };

        vkr = vkQueuePresentKHR(m_presentQueue, &presentInfo);

        if (vkr != VK_SUCCESS)
        {
            std::cout << "Warning: vkQueuePresentKHR failed with result=" << vkr << "\n";
            break;
        }

        frameCount++;
    }

    std::cout << "\n=== Loop finished after " << frameCount << " frames ===\n\n";

    // Wait for device to be idle before cleanup
    if (m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device);
    }

    // Cleanup
    vk_cleanup();

    return 0;
}
