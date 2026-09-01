#include "my_vulkan.h"

#include <iostream>


VkResult vk_get_extensions(uint32_t* numInstanceExtensions)
{
    VkResult vkr = VK_INCOMPLETE;
    std::vector<VkExtensionProperties> instanceExtensionProperties;

    // Query the instance extensions.
    vkEnumerateInstanceExtensionProperties(nullptr,
                                           numInstanceExtensions,
                                           nullptr);

    // If there are any extensions, query their properties.
    if (*numInstanceExtensions != 0)
    {
        instanceExtensionProperties.resize(*numInstanceExtensions);
        vkr = vkEnumerateInstanceExtensionProperties(nullptr,
                                                     numInstanceExtensions,
                                                     instanceExtensionProperties.data());
    }
    return vkr;
}



int main()
{
    // printf("starting window ...\n");
    // if (!glfwInit())
    // {
    //     printf("Failed to initialize GLFW!\n");
    //     return -1;
    // }

    // glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Programming Guide", nullptr, nullptr);
    // if (!window)
    // {
    //     printf("Failed to create GLFW window!\n");
    //     glfwTerminate();
    //     return -1;
    // }
    std::cout << "starting vulkan ...\n";
    my_init_vulkan();
    if(device_count<=0)
    {
        std::cout << "No graphics devices found!\n";
        // glfwDestroyWindow(window);
        // glfwTerminate();
        return 0;
    }
    int active_device_index = 0;
    my_get_device_properties(active_device_index);
    my_get_logical_device(active_device_index);
    my_get_layer_properties(active_device_index);
    uint32_t count = 0;
    VkResult vkr = vk_get_extensions(&count);
    std::cout << count << " extensions found!\n";
    vk_cleanup();
    // glfwDestroyWindow(window);
    // glfwTerminate();
    return 0;
}
