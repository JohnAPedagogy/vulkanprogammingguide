#include "my_vulkan.h"

#include <iostream>



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
    my_get_extensions();
    my_create_buffer();
    vk_cleanup();
    // glfwDestroyWindow(window);
    // glfwTerminate();
    return 0;
}
/*

*/
