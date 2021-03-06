#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>
#include <glm/glm.hpp>
#include <stdexcept>
#include <vector>
#include <fstream>

struct Vertex
{
    glm::vec3 position; //x, y, z
    glm::vec3 color;
};

void create_buffer(const VkPhysicalDevice p_device, VkDevice l_device, VkDeviceSize buffer_size,
                   VkBufferUsageFlags buffer_usage_falgs, VkMemoryPropertyFlags buffer_property_falgs,
                   VkBuffer *vertex_buffer, VkDeviceMemory *vertex_buffer_memory);

void copy_buffer(VkDevice l_device, VkQueue transfer_queue, VkCommandPool transfer_command_pool,
                 VkBuffer src, VkBuffer dst, VkDeviceSize buffer_size);

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
void create_image(const VkPhysicalDevice p_device, VkDevice device, uint32_t width, uint32_t height,
                  VkFormat format, VkImageTiling tiling/*interesting!*/,
                  VkImageUsageFlags use_flags, VkMemoryPropertyFlags mem_flags,
                  VkDeviceMemory &image_memory, VkImage &image);
VkFormat chooseSupportedFormat(const VkPhysicalDevice p_device, const std::vector<VkFormat> &formats, VkImageTiling tiling, VkFormatFeatureFlags feature_flags);
VkShaderModule create_shader_module(VkDevice logical_device, std::vector<char> &shader_code);

//read as binary, spv is binary
inline std::vector<char> read_f(const std::string &filename)
{
    //ate -- put the pointer to the end, toget size
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if(!file.is_open())
    {
        throw std::runtime_error("Failed to open the file: " + filename);
    }

    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<char> file_buffer(file_size);

    file.seekg(0);
    //read the whole file
    file.read(file_buffer.data(), file_size);
    file.close();

    return file_buffer;
}