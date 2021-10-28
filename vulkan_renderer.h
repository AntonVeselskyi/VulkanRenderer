#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <stdexcept>
#include <vector>
#include "vk_utils.h"


class VulkanRenderer
{
public:
    VulkanRenderer() = default;

    int init(GLFWwindow *new_window);

    void cleanup();

    ~VulkanRenderer(){}

private:
    const std::vector<const char*> _needed_device_extentions
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    const std::vector<const char*> _needed_validation_layers { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif

    GLFWwindow *_window;

    // Vulkan components
    //The instance is the connection between your application and the Vulkan library 
    VkInstance _instance;
    struct
    {
        VkPhysicalDevice physical_device; //GPU
        VkDevice logical_device; //Interface to the GPU
        QueueFamilyIndices queue_indecies;

    } _main_device;
    //drawing to our images
    VkQueue _graphics_queue;
    //taking and presenting images to the surface
    VkQueue _presentation_queue;
    VkSurfaceKHR _surface;
    //swapchain stuff
    VkSwapchainKHR _swapchain;
    
    //info needed for image views
    VkFormat _swapchain_image_format;
    VkExtent2D _swapchain_extent;
    std::vector<SwapchainImage> _swapchain_images;

    //pipeline
    VkPipelineLayout _pipline_layout;
    VkRenderPass _render_pass;
    VkPipeline _graphics_pipline;

    /// Vulkan functions
    ///Checks
    //if Vulkan supports needed extensions
    bool check_instance_extensions_support(const std::vector<const char*> &extensions_to_check);
    bool check_device_extension_support(const VkPhysicalDevice& device, const std::vector<const char*> &extensions_to_check);
    //validation stuff
    bool check_validation_layers_support();
    bool check_device_suitable(const VkPhysicalDevice &device);

    //Creation of stuff
    //find our GPU
    void get_physical_device();
    void create_logical_device();
    void create_instance();
    void create_surface();
    void create_swapchain();
    void create_render_pass();
    void create_graphics_pipeline();
};