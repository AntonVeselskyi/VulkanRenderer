#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>

#include "vulkan_renderer.h"

GLFWwindow *window = nullptr;
VulkanRenderer vk_renderer;

void init_window(std::string w_name = "Please render something!", const int width = 800, const int height = 600)
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

    float angle = 0.f, delta_time = 0.f, last_time = 0.f;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        float now = glfwGetTime();
        delta_time = now - last_time;
        last_time = now;

        angle += 360.f * delta_time;
        if(angle > 360.f)
            angle -= 360.f;

        auto how_much_to_rotate = glm::rotate(glm::mat4(1.f), glm::radians(angle), glm::vec3(0.f, 0.f, 1.f));
        vk_renderer.updateModel(how_much_to_rotate);

        vk_renderer.draw();
    }
    
    vk_renderer.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}