#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <stdexcept>
#include <vector>
#include "vk_utils.h"

//store indices(locations) of queue families
struct QueueFamilyIndices
{
    uint32_t graphics_family = -1;
    uint32_t presentation_family = -1;

    bool is_valid()
    {
        return graphics_family >= 0 && presentation_family >= 0;
    }
};

//helper function to extract QFamilies from device
//(which requests are supported)
QueueFamilyIndices get_queue_families_for_device(const VkPhysicalDevice &device, const VkSurfaceKHR &surface);


struct SwapChainCreationDetails
{
    //image size/extent, etc
    VkSurfaceCapabilitiesKHR surface_capabilities;
    //how colot is stored RGB || GRBA, size of each color, etc.
    std::vector<VkSurfaceFormatKHR> surface_formats;
    //how to queue of Swapchain images too be presented to the surface
    std::vector<VkPresentModeKHR> presentation_modes;

    bool is_valid()
    {
        return !surface_formats.empty() && !presentation_modes.empty();
    }
};

SwapChainCreationDetails get_swapchain_details_for_device(const VkPhysicalDevice& device, const VkSurfaceKHR& surface);

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
        VkPhysicalDevice physical_device;
        VkDevice logical_device;
        QueueFamilyIndices queue_indecies;

    } _main_device;
    VkQueue _graphics_queue;
    VkQueue _presentation_queue;
    VkSurfaceKHR _surface;

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
};