#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <iostream>
#include <vector>

#include "vulkan_renderer.h"

GLFWwindow *window = nullptr;
VulkanRenderer vk_renderer;

void init_window(std::string w_name = "Test Window", const int width = 800, const int height = 600)
{
    std::cout << "DEBUG IS: " <<
#ifdef NDEBUG
        "OFF"
#else
        "ON"
#endif
        << "\n";

    // init GLFW
    glfwInit();

    // set to not work with openGL
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(width, height, w_name.c_str(), nullptr, nullptr);
}

int main()
{
    init_window();

    if(vk_renderer.init(window))
        return EXIT_FAILURE;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }
    
    vk_renderer.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}