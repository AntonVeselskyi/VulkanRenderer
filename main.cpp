#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

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
        using namespace glm;

        glfwPollEvents();

        float now = glfwGetTime();
        delta_time = now - last_time;
        last_time = now;

        angle += 20.f * delta_time;
        if(angle > 360.f)
            angle -= 360.f;


        mat4 first_model(1.f);
        first_model = glm::translate(first_model, vec3(0.f, 0.f, -2.2f));
        first_model = glm::rotate(first_model, glm::radians(angle), glm::vec3(0.f, 0.f, 1.f));


        mat4 scnd_model(1.f);
        scnd_model = glm::translate(scnd_model, vec3(0.f, 0.f, -2.9f));
        scnd_model = glm::rotate(scnd_model, glm::radians(-angle * 3), glm::vec3(0.f, 0.f, 1.f));

        vk_renderer.updateModel(0, first_model);
        vk_renderer.updateModel(1, scnd_model);

        vk_renderer.draw();
    }
    
    vk_renderer.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}