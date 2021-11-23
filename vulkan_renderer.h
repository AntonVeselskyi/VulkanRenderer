#pragma once

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <stdexcept>
#include <vector>

#include "vk_utils.h"
#include "vk_mesh.h"


class VulkanRenderer
{
public:
    VulkanRenderer() = default;

    int init(GLFWwindow *new_window);

    void updateModel(uint32_t model_id, glm::mat4 new_model)
    {
        if(model_id >= _meshes.size())
            return;
        
        _meshes[model_id].set_model(new_model);
    }

    void draw();
    void cleanup();

    ~VulkanRenderer(){}

private:
    //max amount of images on the queue
    static constexpr uint32_t MAX_FRAME_DRAWS = 2;
    static constexpr uint32_t MAX_OBJECTS = 20;

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
    uint32_t _current_frame = 0;

    // Scene objects
    std::vector<Mesh> _meshes;

    //Scene settings
    struct UBOViewProjection
    {
        //how camera view the world
        glm::mat4 projection;
        //from where camera is looking at object
        glm::mat4 view;
    } _ubo_vp;

    /// Uniform data
    //How to organilze UBO properly (descripe it to the shader)
    VkDescriptorSetLayout _descriptor_set_layout;

    //one for each command buffer / swapchain image
    //raw data to which descriptor sets will point
    std::vector<VkBuffer> _vp_uniform_buffer;
    std::vector<VkDeviceMemory> _vp_uniform_buffer_memory;

    VkDescriptorPool _descriptor_pool;
    //Describe set of data stored in buffer
    std::vector <VkDescriptorSet> _descriptor_sets;

    VkDeviceSize _min_uniform_buf_offset ;
    size_t _model_uniform_alignment;
    size_t _model_transfer_space_size;
    UBOModel *_model_transfer_space;
    //Dynamic uniform buffer
    std::vector<VkBuffer> _model_uniform_buffer;
    std::vector<VkDeviceMemory> _model_uniform_buffer_memory;

    // Vulkan components
    //The instance is the connection between your application and the Vulkan library 
    VkInstance _instance;
    struct
    {
        VkPhysicalDevice physical_device; //GPU
        VkDevice logical_device; //Interface to the GPU
        QueueFamilyIndices queue_indicies;

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
    //one framebuffer for each swapchain image
    std::vector<VkFramebuffer> _swapchain_framebuffers;
    //one to one connection between -->
    //SwapchainImage <--> VkFramebuffer <--> VkCommandBuffer
    std::vector<VkCommandBuffer> _command_buffers;

    //pipeline
    VkPipelineLayout _pipline_layout;
    VkRenderPass _render_pass;
    VkPipeline _graphics_pipline;

    //pools
    VkCommandPool _graphics_command_pool;

    //synchronization
    std::vector<VkSemaphore> _image_available;
    std::vector<VkSemaphore> _render_finished;
    std::vector<VkFence> _draw_fences;

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
    void create_descriptor_set_layout();
    void create_graphics_pipeline();
    void create_framebuffers();
    void create_command_pool();
    void create_command_buffers();
    void create_synchronization();

    void create_uniform_buffers();
    void create_descriptor_pool();
    void create_descriptor_sets();
    void allocate_dynamic_buffers_transfer_space();


    //record
    void record_commands();

    void update_uniform_buffers(uint32_t index);
};