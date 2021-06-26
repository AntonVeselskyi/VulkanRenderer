#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <stdexcept>
#include <vector>

//store indices of queue families
struct QueueFamilyIndeces
{
    uint32_t graphics_family = -1;

    bool is_valid()
    {
        return graphics_family >= 0;
    }
};

//helper function to extract QFamalies from device
//(whicj requests are supported)
QueueFamilyIndeces get_queue_families_for_device(const VkPhysicalDevice &device);


class VulkanRenderer
{
public:
    VulkanRenderer() = default;

    int init(GLFWwindow *new_window);

    void cleanup();

    ~VulkanRenderer(){}

private:
    const std::vector<const char*> needed_validation_layers { "VK_LAYER_KHRONOS_validation" };
#ifdef NDEBUG
    const bool enable_validation_layers = false;
#else
    const bool enable_validation_layers = true;
#endif

    GLFWwindow *_window;

    // Vulkan components
    //The instance is the connection between your application and the Vulkan library 
    VkInstance instance;
    struct
    {
        VkPhysicalDevice physical_device;
        VkDevice logical_device;
        QueueFamilyIndeces queue_indecies;

    } main_device;
    VkQueue graphics_queue;
    
    // Vulkan functions
    //find our GPU
    void get_physical_device();
    
    bool check_device_suitable(const VkPhysicalDevice &device);

    //if Vulkan supports needed extensions
    bool check_instance_extensions_support(const std::vector<const char*> &extensions_to_check);

    void create_logical_device();
    void create_instance();

    //validation stuff
    bool check_validation_layers_support();
};