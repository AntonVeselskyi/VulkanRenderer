#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <stdexcept>
#include <vector>

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

//Choose functions
VkSurfaceFormatKHR choose_best_surface_format(const std::vector<VkSurfaceFormatKHR> &formats);
VkPresentModeKHR choose_best_presentation_mode(const std::vector<VkPresentModeKHR> &modes);
VkExtent2D choose_best_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window);

struct SwapchainCreationDetails
{
    //image size/extent, etc
    VkSurfaceCapabilitiesKHR surface_capabilities;
    //how colors are stored RGB || GRBA, size of each color, etc.
    std::vector<VkSurfaceFormatKHR> surface_formats;
    //how to queue of Swapchain images too be presented to the surface
    std::vector<VkPresentModeKHR> presentation_modes;

    bool is_valid()
    {
        return !surface_formats.empty() && !presentation_modes.empty();
    }
};

SwapchainCreationDetails get_swapchain_details_for_device(const VkPhysicalDevice &device, const VkSurfaceKHR &surface);

struct SwapchainImage
{
    //access to the existing image
    VkImage image;
    //something we create ourselfs
    VkImageView image_view;
};

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags);
