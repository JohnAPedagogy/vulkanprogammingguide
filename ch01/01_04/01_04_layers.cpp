#include "my_vulkan.h"
//VALIDATION_Layers = "VK_LAYER_KHRONOS_validation"
//DEFINES += ENABLE_VALIDATION

VkResult vk_get_layer_propeties(uint32_t *numInstanceLayers)
{
    VkResult vkr = VK_INCOMPLETE;
    VkLayerProperties* instanceLayerProperties = nullptr;

    // Query the instance layers.
    vkEnumerateInstanceLayerProperties(numInstanceLayers,
                                       nullptr);

    // If there are any layers, query their properties.
    if (numInstanceLayers != 0)
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
    return vkr;
}
int main()
{
    printf("starting window ...\n");
    if (!glfwInit())
    {
        printf("Failed to initialize GLFW!\n");
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Programming Guide", nullptr, nullptr);
    if (!window)
    {
        printf("Failed to create GLFW window!\n");
        glfwTerminate();
        return -1;
    }
    printf("starting vulkan ...\n");
    my_init_vulkan();
    if(device_count<=0)
    {
        printf("No graphics devices found!\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }
    my_get_device_properties(0);
    my_get_logical_device(0);
    vk_cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
