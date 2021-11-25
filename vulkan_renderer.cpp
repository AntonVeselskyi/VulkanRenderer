#include "vulkan_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstring>
#include <set>
#include <array>
#include <algorithm>

#include "io_utils.h"

int VulkanRenderer::init(GLFWwindow *new_window)
{
    _window = new_window;

    try
    {
        create_instance();
        create_surface();
        get_physical_device();
        create_logical_device();
        //get our graphics queue (VkQueue) from logical device
        //place references to the logical device -> queue family -> specific queue index into VK Queue
        vkGetDeviceQueue(_main_device.logical_device, _main_device.queue_indicies.graphics_family, 0, &_graphics_queue);
        vkGetDeviceQueue(_main_device.logical_device, _main_device.queue_indicies.presentation_family, 0, &_presentation_queue);
        //so far we checked that device supports presenting to our surface and created the queue that allows us to do that
        
        create_swapchain();
        create_depth_buffer_image();
        create_render_pass();
        //_descriptor_set_layout needed by pipline
        create_descriptor_set_layout();
        create_push_constant_range();
        create_graphics_pipeline();
        create_framebuffers();
        create_command_pool();
        create_command_buffers();
        //UBO stuff
        //dynamic buffer stuff -- redundant
        //allocate_dynamic_buffers_transfer_space();
        create_uniform_buffers();
        create_descriptor_pool();
        create_descriptor_sets();

        _ubo_vp.projection = glm::perspective(glm::radians(45.f), //setting th angle of Y axis of the camera
                                           float(_swapchain_extent.width)/float(_swapchain_extent.height), //aspect ratio
                                           0.1f, //how close we can see
                                           100.f //how far we can see
                                           );
        //invert Y-axis, cause glm works with OpenGL coordinates, and in Vulkan Y is different
        //flip the scale
        _ubo_vp.projection[1][1] *= -1;
        //view -- where canmera is and how it view the world
        _ubo_vp.view = glm::lookAt(glm::vec3(1.f, 1.f, 1.f), //eye -- where camera 
                                glm::vec3(0.f, 0.f, -4.f), //center -- target of what we looking it, origin of the 
                                glm::vec3(0.f, 1.f, 2.f) //up -- how to orientate
                                );

        //create a mesh
        //vertex data
        std::vector<Vertex> mesh_vertices =
		{
			{{-0.4,-0.2,0.0}, {1.,0.,0.}}, //0 shared
			{{0.0,0.6,0.0}, {1.,0.,0.}}, //1 shared
			{{-0.8,0.2,0.0}, {1.,1.,0.}}, //2
			{{-0.8,-0.2,0.0}, {1.,1.,0.}}, //3
        };
		std::vector<Vertex> mesh_vertices2 =
        {
            {{0.4,0.2,0.0}, {0.,1.,0.}}, //0 shared
            {{0.0,-0.6,0.0}, {0.,1.,0.}}, //1 shared
            {{0.8,-0.2,0.0}, {0.,1.,0.}}, //2
            {{0.8,0.2,0.0}, {1.,1.,0.}}, //3
        };
        //index data
        std::vector<uint32_t> mesh_indices =
        {
            0, 1, 2,
            2, 3, 0
        };
        Mesh mesh = Mesh(_main_device.physical_device, _main_device.logical_device,
                           _graphics_queue, _graphics_command_pool,
                           mesh_vertices, mesh_indices);
        _meshes.push_back(mesh);
        
        mesh = Mesh(_main_device.physical_device, _main_device.logical_device,
                           _graphics_queue, _graphics_command_pool,
                           mesh_vertices2, mesh_indices);
        _meshes.push_back(mesh);


        create_synchronization();
    }
    catch (std::runtime_error &e)
    {
        std::cerr << "fuck error: " <<  e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void VulkanRenderer::draw()
{
    //wait for the previous frame with same index to be submitted and drawn
    //(like  mutex, it`s locked here and unlicked at the end of this function, and checked at the start)
    vkWaitForFences(_main_device.logical_device, 1, &_draw_fences[_current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max());
    //manually lock(reset) fence
    vkResetFences(_main_device.logical_device, 1, &_draw_fences[_current_frame]);

    // 1. Get a next available image to draw 
    // to and set something to signal whem we finished with the image
    //index of the next image to draw to
    uint32_t image_index;
    vkAcquireNextImageKHR(_main_device.logical_device, _swapchain, std::numeric_limits<uint64_t>::max(),
                          _image_available[_current_frame], VK_NULL_HANDLE, &image_index);

    record_commands(image_index);
    update_uniform_buffers(image_index);

    // 2. Submit command buffer to queue for execution,
    // make sure it waits for image to be signaled as available,
    // also signal when it`s draw
    VkPipelineStageFlags wait_stages[]
    {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkSubmitInfo submit_info
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        //check this semaphore right before drawing 
        .pWaitSemaphores = &_image_available[_current_frame],
        //check semaphore at this stage 
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        //every command buffer as individual frame
        .pCommandBuffers = &_command_buffers[image_index],
        //number of semaphores to signal when command buffer finished
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &_render_finished[_current_frame] // after signaled -- we are ready to present
    };

    //hey GPU execute all this commands for me
    //after submited and finished drawing, signal the fence
    VkResult res = vkQueueSubmit(_graphics_queue, 1, &submit_info, _draw_fences[_current_frame]);
    if(res != VK_SUCCESS)
    {
        std::cerr << "VkResult == " << res << std::endl;
        throw std::runtime_error("Failed submit command buffer to the queue!");
    }

    // 3. Present image to screen whem it has signalled finished rendering
    VkPresentInfoKHR present_info
    {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        //present image to the screen after semaphore is signaled
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &_render_finished[_current_frame],
        //present from swapchain
        .swapchainCount = 1,
        .pSwapchains = &_swapchain,
        //which specific image to present
        .pImageIndices = &image_index
    };
    
    res = vkQueuePresentKHR(_presentation_queue, &present_info);
    if(res != VK_SUCCESS)
    {
        std::cerr << "VkResult == " << res << std::endl;
        throw std::runtime_error("Failed to present image to the queue!");
    }

    //increment frame
    _current_frame = (_current_frame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::cleanup()
{
    //wait until device is not doing anything
    //(nothing left on the queue)
    vkDeviceWaitIdle(_main_device.logical_device);
    
    vkDestroyImageView(_main_device.logical_device, _depth_buffer_image_view, nullptr);
    vkDestroyImage(_main_device.logical_device, _depth_buffer_image, nullptr);
    vkFreeMemory(_main_device.logical_device, _depth_buffer_memory, nullptr);

    //dynamic buffer stuff -- redundant
    //_aligned_free(_model_transfer_space);

    vkDestroyDescriptorPool(_main_device.logical_device, _descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(_main_device.logical_device, _descriptor_set_layout, nullptr);
    for(size_t i = 0; i < _vp_uniform_buffer.size(); ++i)
    {
        vkDestroyBuffer(_main_device.logical_device, _vp_uniform_buffer[i], nullptr);
        vkFreeMemory(_main_device.logical_device, _vp_uniform_buffer_memory[i], nullptr);
        //dynamic buffer stuff -- redundant
        //vkDestroyBuffer(_main_device.logical_device, _model_uniform_buffer[i], nullptr);
        //vkFreeMemory(_main_device.logical_device, _model_uniform_buffer_memory[i], nullptr);
    }

    for(auto mesh : _meshes)
        mesh.destroy_buffers();
    for(auto fence : _draw_fences)
        vkDestroyFence(_main_device.logical_device, fence, nullptr);
    for(auto semaphore : _image_available)
        vkDestroySemaphore(_main_device.logical_device, semaphore, nullptr);
    for(auto semaphore : _render_finished)
        vkDestroySemaphore(_main_device.logical_device, semaphore, nullptr);

    vkDestroyCommandPool(_main_device.logical_device, _graphics_command_pool, nullptr);
    for(auto &framebuffer : _swapchain_framebuffers)
        vkDestroyFramebuffer(_main_device.logical_device, framebuffer, nullptr);

    vkDestroyPipeline(_main_device.logical_device, _graphics_pipline, nullptr);
    vkDestroyPipelineLayout(_main_device.logical_device, _pipline_layout, nullptr);
    vkDestroyRenderPass(_main_device.logical_device, _render_pass, nullptr);

    //images are destroyed by the swapchain, but image views clean up is up to us
    for(auto &image : _swapchain_images)
        vkDestroyImageView(_main_device.logical_device, image.image_view, nullptr);
    //Everytime we do a create we need to do a destroy
    vkDestroySwapchainKHR(_main_device.logical_device, _swapchain, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyDevice(_main_device.logical_device, nullptr);
    vkDestroyInstance(_instance, nullptr);
}


void VulkanRenderer::get_physical_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);

    //are there any devices
    if(!device_count)
        throw std::runtime_error("No GPU compatable with Vulkan found");

    std::vector<VkPhysicalDevice> device_list(device_count);
    vkEnumeratePhysicalDevices(_instance, &device_count, device_list.data());

    //just list GPUs out of curiosity
    std::cout << bold_on << "Available Devices:\n" << bold_off;
    for(auto dev: device_list)
    {
        VkPhysicalDeviceProperties device_props;
        vkGetPhysicalDeviceProperties(dev, &device_props);
        std::cout << device_props.deviceName << std::endl;
    }

    for(const auto &dev : device_list)
        if(check_device_suitable(dev))
        {
            _main_device.physical_device = dev;
            //after device is chosen, lets save queue family index
            _main_device.queue_indicies =  get_queue_families_for_device(dev, _surface);
            break;
        }
}

bool VulkanRenderer::check_device_suitable(const VkPhysicalDevice &device)
{
    /*
    //Info about what the device can do
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device, &device_features);
    */
    const bool queues_are_valid = get_queue_families_for_device(device, _surface).is_valid();
    const bool extensions_supported = check_device_extension_support(device, _needed_device_extentions);
    const bool swapchain_supported = get_swapchain_details_for_device(device, _surface).is_valid();
    return queues_are_valid && extensions_supported && swapchain_supported;
}

bool VulkanRenderer::check_instance_extensions_support(const std::vector<const char*> &extensions_to_check)
{
    //get number of extensions
    uint32_t extensions_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, nullptr);

    //create list of extension properties
    std::vector<VkExtensionProperties> extensions(extensions_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensions_count, extensions.data());

    //validate extensions passed as argument
    for(const auto &extension_to_check : extensions_to_check)
    {
        bool extension_supported = std::any_of(begin(extensions), end(extensions), [&](const VkExtensionProperties &ext)
        {
            return !strcmp(extension_to_check, ext.extensionName);
        });

        if(!extension_supported)
            return false;
    }

    return true;
}

bool VulkanRenderer::check_device_extension_support(const VkPhysicalDevice& device, const std::vector<const char*>& extensions_to_check)
{
    uint32_t extensions_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensions_count, nullptr);
    if (extensions_count == 0)
        return false;

    std::vector<VkExtensionProperties> extensions(extensions_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensions_count, extensions.data());

    for (const auto& extention : extensions_to_check)
    {
        bool extension_supported = std::any_of(begin(extensions), end(extensions), [&](const VkExtensionProperties& ext)
            {
                return !strcmp(extention, ext.extensionName);
            });

        if (!extension_supported)
            return false;
    }

    return true;
}

void VulkanRenderer::create_logical_device()
{
    //queues logical device need to create
    //graphics and presentation queues (if its the same queue, pass only one)
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> queue_family_indices {_main_device.queue_indicies.graphics_family, _main_device.queue_indicies.presentation_family};
    float priority = 1.f; // 1 - highest priority
    for(uint32_t queue_family_index : queue_family_indices)
    {
        VkDeviceQueueCreateInfo queue_create_info
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = &priority //vulkan needs to know how to handle different queues
        };
        queue_create_infos.push_back(std::move(queue_create_info));
    }

    VkPhysicalDeviceFeatures pd_features{};

    //Device === Logical Device
    VkDeviceCreateInfo device_create_info
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(_needed_device_extentions.size()),
        .ppEnabledExtensionNames = _needed_device_extentions.data(),
        //.enabledLayerCount depricated, handled by instance
        .pEnabledFeatures = &pd_features
    };

    VkResult result = vkCreateDevice(_main_device.physical_device, &device_create_info, nullptr, &_main_device.logical_device);
    if(result != VK_SUCCESS)
    {
        std::cerr << "VkResult == " << result << std::endl;
        throw std::runtime_error("Failed to create a Vulkan Logical Device");
    }
}

void VulkanRenderer::create_instance()
{
    if(enable_validation_layers && !check_validation_layers_support())
        throw std::runtime_error("Validation layers requested, but not available!");

    //info about application itself
    //most of the data is for devs
    VkApplicationInfo app_info =
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        //.pNext = nullptr,
        .pApplicationName = "", //custom app name
        .applicationVersion = VK_MAKE_VERSION(1,0,0),
        .pEngineName = "No Engine", //nonempty if we create engine
        .engineVersion = VK_MAKE_VERSION(1,0,0),
        .apiVersion = VK_API_VERSION_1_2, //VULKAN version
    };


    //get all extensions
    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    //create a list to hold the extensions 
    std::vector<const char*> instance_extensions(glfw_extension_count);
    for(size_t i = glfw_extension_count;  i--; )
        instance_extensions[i] = glfw_extensions[i];

    //check if Vulkan supports needed INSTANCE extensions
    if(!check_instance_extensions_support(instance_extensions))
        throw std::runtime_error("extensions needed by glfw is not supported!");

    //just list extentions out of curiosity
    std::cout << bold_on << "Needed Extentions:\n" << bold_off;
    for(auto ex_str : instance_extensions)
        std::cout << ex_str << std::endl;

    uint32_t validation_layers_count = (uint32_t)(enable_validation_layers ? _needed_validation_layers.size() : 0);
    auto *validation_layers_names = (enable_validation_layers ? _needed_validation_layers.data() : nullptr);

    //.p* - pointer
    //.pp* - pointer to a pointer
    VkInstanceCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = validation_layers_count,
        .ppEnabledLayerNames = validation_layers_names,
        .enabledExtensionCount = glfw_extension_count,
        .ppEnabledExtensionNames = instance_extensions.data(),
        //.pNext = nullptr//void pointer to anything of some extra
    };


    //finally create instance
    VkResult result = vkCreateInstance(&create_info, nullptr, &_instance);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to create a Vulkan Instance");
}

void VulkanRenderer::create_surface()
{
    //platform agnostic call:
    //not relay on physical device, only on surface
    VkResult res = glfwCreateWindowSurface(_instance, _window, nullptr, &_surface);

    //Under the hood for win it calls:
    //#define VK_USE_PLATFORM_WIN32_KHR
    //VkWin32SurfaceCreateInfoKHR();
    //VkCreateWin32SurfaceKHR();
    if(res != VK_SUCCESS)
        throw std::runtime_error("Failed to create a surface!");
}

void VulkanRenderer::create_swapchain()
{
    SwapchainCreationDetails creation_details = get_swapchain_details_for_device(_main_device.physical_device, _surface);
    VkSurfaceFormatKHR s_format = choose_best_surface_format(creation_details.surface_formats);
    VkPresentModeKHR s_present_mode = choose_best_presentation_mode(creation_details.presentation_modes);
    //choose best swapchain image resolution
    VkExtent2D s_extent = choose_best_swap_extent(creation_details.surface_capabilities, _window);

    //How many images are in the swap chain?
    //Get 1 more then min to allow triple buffering
    uint32_t minImageCount = creation_details.surface_capabilities.minImageCount + 1;
    //there is no limit if maxImageCount it`s 0
    if(creation_details.surface_capabilities.maxImageCount > 0)
        minImageCount = std::min(minImageCount, creation_details.surface_capabilities.maxImageCount);
    
    QueueFamilyIndices indices = get_queue_families_for_device(_main_device.physical_device, _surface);
    //if graphics and presentation queues are different,
    //then swapchain must let images to be shared between families
    VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t count_of_family_queues = 0;
    uint32_t *p_family_indices = nullptr;

    //check if presentation and graphics queue are separate
    uint32_t concurent_family_indices[] =
    {
        indices.graphics_family,
        indices.presentation_family
    };
    if(indices.graphics_family != indices.presentation_family)
    {
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
        count_of_family_queues = 2;
        p_family_indices = concurent_family_indices;
    }
    

    VkSwapchainCreateInfoKHR creation_info =
    {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = _surface,

        //at least this number of images in the swapchain to be compatable with a surface
        .minImageCount = minImageCount,
        .imageFormat = s_format.format,
        .imageColorSpace = s_format.colorSpace,
        .imageExtent = s_extent,
        // 1 layer so far for each image
        .imageArrayLayers = 1,
        //will attachemnt images will be used as??
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        
        .imageSharingMode = sharing_mode,
        .queueFamilyIndexCount = count_of_family_queues,
        .pQueueFamilyIndices = p_family_indices,

        .preTransform = creation_details.surface_capabilities.currentTransform,
        //how to handle overlapping with other windows, draw as normally
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = s_present_mode,
        //clip parts of image not at view (under other image, out of screen)
        .clipped = VK_TRUE,
        //if we got new swapchain, and we want to pass responsobilities to the new one
        //useful when resized to update extent
        //resize == destroy swap chaind and create new
        .oldSwapchain = VK_NULL_HANDLE
    };

    VkResult res = vkCreateSwapchainKHR(_main_device.logical_device, &creation_info, nullptr, &_swapchain);
    if(res != VK_SUCCESS)
        throw std::runtime_error("Failed to create a swapchain!");

    //save format and extent for further reuse (after successful creation)
    _swapchain_image_format = s_format.format;
    _swapchain_extent = s_extent;

    uint32_t swapchain_image_count;
    vkGetSwapchainImagesKHR(_main_device.logical_device, _swapchain, &swapchain_image_count, nullptr);
    std::vector<VkImage> images(swapchain_image_count);
    vkGetSwapchainImagesKHR(_main_device.logical_device, _swapchain, &swapchain_image_count, images.data());


    //go through ids of the images
    std::cout << bold_on << "Swapchain image count: " << bold_off << swapchain_image_count << std::endl;
    _swapchain_images.reserve(swapchain_image_count);
    for(VkImage image : images)
    {
        SwapchainImage sc_image =
        {
            .image = image,
            .image_view =
                create_image_view(_main_device.logical_device, image, _swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT)
        };
        _swapchain_images.push_back(sc_image);
    }

}

void VulkanRenderer::create_render_pass()
{
    //ATTACHMENTS
    //Describe places to output data to and input data from
    VkAttachmentDescription color_attachment
    {
        .format = _swapchain_image_format,
        //number of samples for multisampling
        .samples = VK_SAMPLE_COUNT_1_BIT,
        //operation to perform when first loading color attachment
        //VK_ATTACHMENT_LOAD_OP_CLEAR start with the clear values
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        //when we finished with render pass what to do with all the data
        //VK_ATTACHMENT_STORE_OP_STORE store == save
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

        //Framebuffer data will be stored as image,
        //but images can be given different data layouts
        //to give optimal  use for certain operation
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, //data layout before render pass starts, that we excpect to have already
        //initialLayout --> subpassFormat (process as ATTACHMENT_OPTIMAL) --> finalLayout
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR //after render pass (to convert to)
    };
    VkAttachmentDescription depth_attachment
    {
        .format = _depth_buffer_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        //VK_ATTACHMENT_STORE_OP_DONT_CARE we don`t need this data out of render pass process
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    std::array<VkAttachmentDescription, 2> render_attachments =
    {
        color_attachment, depth_attachment
    };

    //REFS
    //Attachment reference to reference one of the attachment in VkRenderPassCreateInfo
    VkAttachmentReference color_attachment_ref
    {
        //index in the render pass
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depth_attachment_ref
    {
        //index in the render pass
        .attachment = 1,
        //layout between initial and final
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    //Individual subpasses
    VkSubpassDescription subpass
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &depth_attachment_ref
    };

    //subpass dependencies
    //layout changes (transitions between different subpasses)
    std::array<VkSubpassDependency, 2> subpass_dependencies =
    {
        //Initial conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        VkSubpassDependency
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL, //means anything before our subpass (out side of render pass)
            .dstSubpass = 0, // id of our subpass
            
            .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, //stage at the end of pipeline to happen before the transition
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, //make the conversion before color conversion
            
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT, //we read from src
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

            .dependencyFlags = 0
        },
        //Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR(presentation)
        VkSubpassDependency
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL, 
            
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, //we read from src
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            
            .dependencyFlags = 0
        }
    };

    VkRenderPassCreateInfo render_pass_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(render_attachments.size()),
        .pAttachments = render_attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = static_cast<uint32_t>(subpass_dependencies.size()),
        .pDependencies = subpass_dependencies.data()
    };

    VkResult res = vkCreateRenderPass(_main_device.logical_device, &render_pass_createinfo, nullptr, &_render_pass);
    if(res != VK_SUCCESS)
    { 
        throw std::runtime_error("Failled to create render pass!");
    }
}

void VulkanRenderer::create_descriptor_set_layout()
{    
    //bind view-projection data to our shader
    const VkDescriptorSetLayoutBinding vp_layout_binding
    {
        .binding = 0, //binding point from shader
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, //vertex or fragment
        .pImmutableSamplers = nullptr //for textures
    };
    
    //dynamic buffer stuff -- redundant
    ////bind model data to our shader
    //const VkDescriptorSetLayoutBinding model_layout_binding
    //{
    //    .binding = 1, //binding point from shader
    //    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    //    .descriptorCount = 1,
    //    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, //vertex or fragment
    //    .pImmutableSamplers = nullptr //for textures
    //};

    std::array<VkDescriptorSetLayoutBinding, 1> layouts
    {
        vp_layout_binding,
        //dynamic buffer stuff -- redundant
        //model_layout_binding
    };

    VkDescriptorSetLayoutCreateInfo create_info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(layouts.size()),
        .pBindings = layouts.data()
    };

    VkResult res = vkCreateDescriptorSetLayout(_main_device.logical_device, &create_info, nullptr, &_descriptor_set_layout);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create DescriptorSetLayout!");
    }
}

void VulkanRenderer::create_push_constant_range()
{
    _push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    _push_constant_range.offset = 0;
    //size of data being passed
    _push_constant_range.size = sizeof(Model);
}

void VulkanRenderer::create_graphics_pipeline()
{
    auto vertex_shader_code = read_f("shaders/vert.spv");
    auto fragment_shader_code = read_f("shaders/frag.spv");

    //Build shader modules to link to the Graphics pipeline
    VkShaderModule vertex_shader_module = create_shader_module(_main_device.logical_device, vertex_shader_code);
    VkShaderModule fragment_shader_module = create_shader_module(_main_device.logical_device, fragment_shader_code);

    // SHADER STAGE CREATION INFO
    VkPipelineShaderStageCreateInfo vertex_shader_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertex_shader_module,
        //function to run in GLSL
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo fragment_shader_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragment_shader_module,
        //function to run in GLSL
        .pName = "main"
    };

    //Graphics Pipeline requires array of shader info
    VkPipelineShaderStageCreateInfo shader_stages[] =
    {
        vertex_shader_createinfo,
        fragment_shader_createinfo
    };

    //VERTEX INPUT
    //How a data for any 1 vertex (pos, color, texture, normals...) is layout
    VkVertexInputBindingDescription binding_description
    {
        //can bind multiple streams of data, define which one
        //which IN parameter in the vertex shader 
        .binding = 0,
        //size of the individual vertex
        .stride = sizeof(Vertex),
        //How to move the data after each vertex
        //if we draw multiple instances:
        // draw all 1st vertices and thend draw all 2nd...
        // or draw all 1st object vertices, then 2nd...
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    //How data within a vertex is defined
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions =
    {
        //Position description
        VkVertexInputAttributeDescription
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT, //data format
            .offset = offsetof(Vertex, position)
        },
        //ColorDescription
        VkVertexInputAttributeDescription
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, color)
        }
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        //list of vertex binding desc (data spacing, stride informantion)
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
        //(data format and where to bind it)
        .pVertexAttributeDescriptions = attribute_descriptions.data()
    };

    //INPUT ASSEMBLY
    //(where we assembly input into primitives)
    VkPipelineInputAssemblyStateCreateInfo input_assembly_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        //type of primitive to assemble verticies as (triangles, lines) 
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    //VIEWPORT AND SCISSOR
    // viewport -- how the transforming image into screen
    // from top-left to buttom (different for splitscreen for example)
    VkViewport viewport
    {
        //start coordinates
        .x = 0.f,
        .y = 0.f,
        .width = static_cast<float>(_swapchain_extent.width),
        .height = static_cast<float>(_swapchain_extent.height),
        // standard depth trange for Vulkan
        .minDepth = 0.f,
        .maxDepth = 1.f
    };

    //scissor - is basicly which part of the image we cut
    VkRect2D scissor
    {
        .offset = {0, 0},
        .extent = _swapchain_extent
    };

    VkPipelineViewportStateCreateInfo viewport_state_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    // DYNAMIC STATE - not used for now
    // something is not backed but configurable at runtime
    // we define what parts of pipeline is is dynamic(issued in command buffer)
    std::vector<VkDynamicState> dynamic_states
    {
        //can be resized like this vkCmdSetViewport(commandbuffer, 0, 1, &viewport)
        VK_DYNAMIC_STATE_VIEWPORT,
        // can be also resized in command buffer with vkCmdSetScissor(commandbuffer, 0, 1, &scissor)
        VK_DYNAMIC_STATE_SCISSOR
    };

    //Dynamic state creation info
    VkPipelineDynamicStateCreateInfo dynamic_state_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data()
    };

    //RASTERIZER - convert the primitives(triangles) into fragments
    VkPipelineRasterizationStateCreateInfo rasterization_state_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        // when somehing is far away (far plane) do not render them
        // flat everything when it is far: false, GPU feature
        .depthClampEnable = VK_FALSE,
        // if we want to discard data
        // you compute stuff and do not draw
        .rasterizerDiscardEnable = VK_FALSE,
        // how we want to handle poligons
        // other then fill -- you need GPU feature
        .polygonMode = VK_POLYGON_MODE_FILL,
        // do not draw the back of tri
        .cullMode = VK_CULL_MODE_BACK_BIT,
        //define what side is the face (if we draw the 3 point in the counter-clockwize)
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        //to fix "shadow acne"? add a little bit to depth for shadow calculation to fragmenths
        .depthBiasEnable = VK_FALSE,
        // how thick lines would be
        .lineWidth = 1.f,

    };

    // MULTISAMPLING -- form of antialiasing
    // not for textures, fix "stairs" of the sides of triangles
    VkPipelineMultisampleStateCreateInfo multisampling_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        //num of samples to use per fragment
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        //disable the whole feature for now
        .sampleShadingEnable = VK_FALSE,
    };

    // BLENDING - of two fragments together
    // (you draw an object and another object on top of it)
    // how to blend new color of fragment with the existing one

    // Blend attachment state -- how blending is handled
    //Blending equasion: (srcColorBlendFactor * newCOLOR) colorBlendOp (dstColorBlendFactor * oldCOLOR)
    //Our Case: (VK_BLEND_FACTOR_SRC_ALPHA * newCOLOR) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * oldCOLOR)
    //Or: (newCOLOR-ALPHA * newCOLOR) + ((1-newCOLOR-ALPHA) * oldCOLOR)
    //in short: use alpha of the new color!
    VkPipelineColorBlendAttachmentState how_to_blend_colors
    {
        .blendEnable = VK_TRUE,
        //how to blend colors
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        //math op to do
        .colorBlendOp = VK_BLEND_OP_ADD,
        //how to blend alpha:
        //take new alpha
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        //get rid of the old alpha
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        //same equasion as color 
        .alphaBlendOp = VK_BLEND_OP_ADD,
        //what colors to be changed by blending
        .colorWriteMask =
            (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
    };
    
    VkPipelineColorBlendStateCreateInfo color_blending_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        //logic operation instead of calculation of blended color
        .logicOpEnable = VK_FALSE,
        //color blend attachment state to multiply colors together
        .attachmentCount = 1,
        .pAttachments = &how_to_blend_colors
        
    };

    //PIPLINE LAYOUT
    VkPipelineLayoutCreateInfo layout_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &_descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &_push_constant_range
    };

    //set up depth stencil testing
    VkPipelineDepthStencilStateCreateInfo depth_satencil_create_info
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        //when false - by default depth is 1 (as far away as possible
        //when true - test every pixel depth with existing pixel(fragment) before drawing it
        .depthTestEnable = VK_TRUE,
        //when we put a new pixel we update the depth buffer with relevant info
        .depthWriteEnable = VK_TRUE,
        //if new_value < old ==> overwrite fragment
        .depthCompareOp = VK_COMPARE_OP_LESS,
        //check if depth value in between two values
        .depthBoundsTestEnable = VK_FALSE,
        //we don`t want to enable a stencil test
        .stencilTestEnable = VK_FALSE
    };

    VkResult res = vkCreatePipelineLayout(_main_device.logical_device, &layout_createinfo, nullptr, &_pipline_layout);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a pipeline layout!");
    }
    //PIPLINE
    //Create pipline
    VkGraphicsPipelineCreateInfo pipline_createinfo
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_createinfo,
        .pInputAssemblyState = &input_assembly_createinfo,
        .pViewportState = &viewport_state_createinfo,
        .pRasterizationState = &rasterization_state_createinfo,
        .pMultisampleState = &multisampling_createinfo,
        .pDepthStencilState = &depth_satencil_create_info,
        .pColorBlendState = &color_blending_createinfo,
        .pDynamicState = nullptr,
        .layout = _pipline_layout,
        .renderPass = _render_pass, //render pass that will be used by the pipline
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE, //if you want to base new pipline on the old one 
        .basePipelineIndex = -1 // index of the base pipline if you create multiple piplines
    };

    //there is also a way to chache the pipline_createinfo,
    res = vkCreateGraphicsPipelines(_main_device.logical_device, VK_NULL_HANDLE, 1, &pipline_createinfo, nullptr, &_graphics_pipline);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a graphics pipeline!");
    }

    //Clean up of the not needed shader modules after we create pipeline
    vkDestroyShaderModule(_main_device.logical_device, fragment_shader_module, nullptr);
    vkDestroyShaderModule(_main_device.logical_device, vertex_shader_module, nullptr);

}

void VulkanRenderer::create_depth_buffer_image()
{
    //d32 - depth buffer of 32bits, s8 -- stencil buffer
    _depth_buffer_format = chooseSupportedFormat(_main_device.physical_device,
                                                 {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
                                                 VK_IMAGE_TILING_OPTIMAL,
                                                 VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    
    create_image(_main_device.physical_device, _main_device.logical_device,
                 _swapchain_extent.width, _swapchain_extent.height,
                 _depth_buffer_format, VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 _depth_buffer_memory, _depth_buffer_image);

    //wea re going to interact with DEPTH aspect of this image
    _depth_buffer_image_view = create_image_view(_main_device.logical_device, _depth_buffer_image, _depth_buffer_format, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::create_framebuffers()
{
    _swapchain_framebuffers.resize(_swapchain_images.size());
    //create a framebuffer for every image
    for(size_t i = 0; i < _swapchain_framebuffers.size(); i++)
    {
        std::array<VkImageView, 2> attachments =
        {
            _swapchain_images[i].image_view, _depth_buffer_image_view
        }; 

        VkFramebufferCreateInfo fb_createinfo
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            //render pass layout the framebuffer will be used with
            .renderPass = _render_pass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            //not the description of color attachment, but the image we actually attach
            .pAttachments = attachments.data(),
            .width = _swapchain_extent.width,
            .height = _swapchain_extent.height,
            //only one layer
            .layers = 1
        };

        VkResult res = vkCreateFramebuffer(_main_device.logical_device, &fb_createinfo, nullptr, &_swapchain_framebuffers[i]);
        if(res != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanRenderer::create_command_pool()
{
    //chunk of memory dedicated only for creation of command buffers
    VkCommandPoolCreateInfo command_pool_createinfo
    {

        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        //command buffer can be reset for rerecord
        //on vkBeginCommandBuffer -- do a reset
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        //in which queue command buffers will be used
        .queueFamilyIndex = _main_device.queue_indicies.graphics_family,
    };
    VkResult res = vkCreateCommandPool(_main_device.logical_device, &command_pool_createinfo, nullptr, &_graphics_command_pool);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a command buffer pool!");
    }
}

void VulkanRenderer::create_command_buffers()
{
    _command_buffers.resize(_swapchain_framebuffers.size());

    VkCommandBufferAllocateInfo cb_alloc_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = _graphics_command_pool,
        //not primary -- command buffer in command buffer
        //primary call secondary
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(_command_buffers.size())
    };

    VkResult res = vkAllocateCommandBuffers(_main_device.logical_device, &cb_alloc_info, _command_buffers.data());
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void VulkanRenderer::create_synchronization()
{
    _image_available.resize(MAX_FRAME_DRAWS);
    _render_finished.resize(MAX_FRAME_DRAWS);
    _draw_fences.resize(MAX_FRAME_DRAWS);


    VkSemaphoreCreateInfo semaphore_create_info
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo fence_create_info
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        //fence to be unlocked by default (signaled, open)
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for(size_t i = 0; i < MAX_FRAME_DRAWS; i++)
        if
        (
            vkCreateSemaphore(_main_device.logical_device, &semaphore_create_info, nullptr, &_image_available[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_main_device.logical_device, &semaphore_create_info, nullptr, &_render_finished[i]) != VK_SUCCESS ||
            vkCreateFence(_main_device.logical_device, &fence_create_info, nullptr, &_draw_fences[i]) != VK_SUCCESS
        )
        {
            throw std::runtime_error("Failed to create semaphores/fence!");
        }
}

void VulkanRenderer::create_uniform_buffers()
{
    const VkDeviceSize vp_buffer_size = sizeof(UBOViewProjection);
    ////dynamic buffer stuff -- redundant
    //const VkDeviceSize  model_buffer_size = _model_transfer_space_size;

    //One uniform buffer for each image
    const size_t buffers_num = _swapchain_images.size();

    _vp_uniform_buffer.resize(buffers_num);
    _vp_uniform_buffer_memory.resize(buffers_num);
    
    //dynamic buffer stuff -- redundant
    //_model_uniform_buffer.resize(buffers_num);
    //_model_uniform_buffer_memory.resize(buffers_num);

    //Create ViewProjection uniform buffers and
    //Create Model dynamic unified buffers
    for(size_t i = 0; i < buffers_num; ++i)
    {
        create_buffer(_main_device.physical_device, _main_device.logical_device, vp_buffer_size,
                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, //memory visible only to the GPU 
                  &_vp_uniform_buffer[i], &_vp_uniform_buffer_memory[i]);
       
        //dynamic buffer stuff -- redundant
        //create_buffer(_main_device.physical_device, _main_device.logical_device, model_buffer_size,
        //              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        //              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, //memory visible only to the GPU 
        //              &_model_uniform_buffer[i], &_model_uniform_buffer_memory[i]);
    }
}

void VulkanRenderer::create_descriptor_pool()
{
    //ViewProjection pool
    VkDescriptorPoolSize vp_pool_size
    {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        //each set has 1 descriptor (mvp struct)
        .descriptorCount = static_cast<uint32_t>(_vp_uniform_buffer.size())
    };

    //dynamic buffer stuff -- redundant
    //Model pool
    //VkDescriptorPoolSize model_pool_size
    //{
    //    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    //    //each set has 1 descriptor (mvp struct)
    //    .descriptorCount = static_cast<uint32_t>(_model_uniform_buffer.size())
    //};

    std::array<VkDescriptorPoolSize, 1> pool_sizes
    {
        vp_pool_size,
        //model_pool_size
    };

    VkDescriptorPoolCreateInfo create_info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = static_cast<uint32_t>(_swapchain_images.size()),
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };

    VkResult res = vkCreateDescriptorPool(_main_device.logical_device, &create_info, nullptr, &_descriptor_pool);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Descriptor Pool!");
    }
}

void VulkanRenderer::create_descriptor_sets()
{
    //1. Create sets
    const uint32_t buffers_num = static_cast<uint32_t>(_swapchain_images.size());
    _descriptor_sets.resize(buffers_num);

    //each set has a same layout
    std::vector<VkDescriptorSetLayout> set_layouts(buffers_num, _descriptor_set_layout);

    //we create multiple sets at once
    VkDescriptorSetAllocateInfo allocate_info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = _descriptor_pool,
        .descriptorSetCount = buffers_num,
        .pSetLayouts = set_layouts.data()
    };

    VkResult res = vkAllocateDescriptorSets(_main_device.logical_device, &allocate_info, _descriptor_sets.data());
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Descriptor Sets!");
    }

    //2. Connect each set to its uniform buffer
    for(size_t i = 0; i < buffers_num; ++i)
    {
        //VIEW-PROJECTION
        VkDescriptorBufferInfo vp_buffer_info
        {
            .buffer = _vp_uniform_buffer[i],
            //start of data == start of the buffer
            .offset = 0,
            //how much data will be bound
            .range = sizeof(UBOViewProjection)
        };

        VkWriteDescriptorSet vp_set_write
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = _descriptor_sets[i],
            //ref to shader: layout(binding = 0) uniform 
            .dstBinding = 0,
            //index in array of element
            .dstArrayElement = 0,
            //how many descriptors are we updating
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &vp_buffer_info
        };

        //dynamic buffer stuff -- redundant
        ////MODEL
        //VkDescriptorBufferInfo model_buffer_info
        //{
        //    .buffer = _model_uniform_buffer[i],
        //    //start of data == start of the buffer
        //    .offset = 0,
        //    //step
        //    .range = _model_uniform_alignment
        //};

        //VkWriteDescriptorSet model_set_write
        //{
        //    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        //    .dstSet = _descriptor_sets[i],
        //    //ref to shader: layout(binding = 1) uniform 
        //    .dstBinding = 1,
        //    //index in array of element
        //    .dstArrayElement = 0,
        //    //how many descriotors are we updating
        //    .descriptorCount = 1,
        //    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        //    .pBufferInfo = &model_buffer_info
        //};

        std::array<VkWriteDescriptorSet, 1> descriptor_set_updates
        {
            vp_set_write,
            //dynamic buffer stuff -- redundant
            //model_set_write
        };

        vkUpdateDescriptorSets(_main_device.logical_device,
                               descriptor_set_updates.size(), descriptor_set_updates.data(),
                               0, nullptr);
    }
}

//dynamic buffer stuff -- redundant
//void VulkanRenderer::allocate_dynamic_buffers_transfer_space()
//{
//    //Info about device itself
//    VkPhysicalDeviceProperties device_props;
//    vkGetPhysicalDeviceProperties(_main_device.physical_device, &device_props);
//    _min_uniform_buf_offset = device_props.limits.minUniformBufferOffsetAlignment;
//
//    //Calculate model uniform alighment
//    if(sizeof(Model) == _min_uniform_buf_offset)
//        _model_uniform_alignment = _min_uniform_buf_offset;
//    else
//        _model_uniform_alignment = (sizeof(Model) + _min_uniform_buf_offset) & ~(_min_uniform_buf_offset - 1);
//
//    //Peace of data to store all Model UBOs
//    //Allocate a lot of memory and then add/delete meshes
//    _model_transfer_space_size = _model_uniform_alignment * MAX_OBJECTS;
//    _model_transfer_space = (Model *)_aligned_malloc(_model_transfer_space_size, _model_uniform_alignment);
//}

void VulkanRenderer::record_commands(const uint32_t current_image)
{
    //Info about how to begin each command buffer
    VkCommandBufferBeginInfo cb_begin_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        //commented out, cause we use fences now
        //can two the same command buffers be on the same queue the the same time 
        //.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
    };

    //there will be more values
    std::array<VkClearValue, 2>  clear_values =
    {
        //colors we clear the screen with
        VkClearValue{.color = {0.6f, 0.65f, 0.4f, 1.f}}, //clear the first attachment with this color
        VkClearValue{.depthStencil = {.depth = 1.f }} //depth
    };

    VkRenderPassBeginInfo  rp_begin_info
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        //which render pass we are begining
        .renderPass = _render_pass,
        .renderArea =
        {
            .offset = {0,0}, //start point of render pass in pixels
            .extent = _swapchain_extent //size of region to run render pass at (starting at offset)
        },
        .clearValueCount = static_cast<uint32_t>(clear_values.size()),
        .pClearValues = clear_values.data()
    };


    //start recording commands into command buffer
    VkResult res = vkBeginCommandBuffer(_command_buffers[current_image], &cb_begin_info);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to start recording a command buffer!");
    }

    //everything with vkCmd is recorded commands
    {
        //say we are using a render pass (not compute or transfer)
        rp_begin_info.framebuffer = _swapchain_framebuffers[current_image];
        vkCmdBeginRenderPass(_command_buffers[current_image], &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        //INLINE -- no secoonary command buffers

        //bind pipeline to render pass
        vkCmdBindPipeline(_command_buffers[current_image], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipline);

        size_t j = 0;
        for(Mesh &mesh : _meshes)
        {
            //Buffers to bind to drawing
            VkBuffer vertex_buffers[] = {mesh.get_vertex_buffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(_command_buffers[current_image], 0/*binding from shader*/, 1, vertex_buffers, offsets);
            vkCmdBindIndexBuffer(_command_buffers[current_image], mesh.get_index_buffer(), 0, VK_INDEX_TYPE_UINT32);

            //dynamic buffer stuff -- redundant
            //Offset only applies to the dynamic uniform buffers
            //const uint32_t dynamic_model_offset_for_current_mesh = _model_uniform_alignment * j;

            //Push constants to vertex directly without a buffer
            vkCmdPushConstants(_command_buffers[current_image],
                               _pipline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(Model),
                               &mesh.get_model()
                               );
            vkCmdBindDescriptorSets(_command_buffers[current_image], VK_PIPELINE_BIND_POINT_GRAPHICS, _pipline_layout,
                                    0/*first set*/, 1, &_descriptor_sets[current_image], 0, nullptr);
            //dynamic buffer stuff -- redundant
                                    //1, &dynamic_model_offset_for_current_mesh);

            //execute our pipline
            vkCmdDrawIndexed(_command_buffers[current_image], mesh.get_index_count(), 1, 0, 0, 0);
                
            j++;
        }

        vkCmdEndRenderPass(_command_buffers[current_image]);

        res = vkEndCommandBuffer(_command_buffers[current_image]);
        if(res != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to stop recording a command buffer!");
        }
    }
}

//Update date about view and position of all objects every frame
void VulkanRenderer::update_uniform_buffers(uint32_t index)
{
    //VP data
    void *data = nullptr;
    vkMapMemory(_main_device.logical_device, _vp_uniform_buffer_memory[index], 0, sizeof(UBOViewProjection), 0, &data);
    std::memcpy(data, &_ubo_vp, sizeof(_ubo_vp));
    vkUnmapMemory(_main_device.logical_device, _vp_uniform_buffer_memory[index]);

    //Model data
    //Was relevant when we used dynamic buffers, keep here as a reference
    //for(size_t i = 0; i < _meshes.size(); i++)
    //{
    //    const uint32_t dynamic_model_offset_for_current_mesh = _model_uniform_alignment * i;
    //    Model *current_model = (Model*)((uint64_t)_model_transfer_space + dynamic_model_offset_for_current_mesh);
    //    *current_model = _meshes[i].get_model();
    //}
    //vkMapMemory(_main_device.logical_device, _model_uniform_buffer_memory[index], 0, _meshes.size() * _model_uniform_alignment, 0, &data);
    //std::memcpy(data, _model_transfer_space, _meshes.size() * _model_uniform_alignment);
    //vkUnmapMemory(_main_device.logical_device, _model_uniform_buffer_memory[index]);
}

bool VulkanRenderer::check_validation_layers_support()
{
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    //just list Available Validation Layers out of curiosity
    std::cout << bold_on << "Available Validation Layers:\n" << bold_off;
    for(auto &layer_properties : available_layers)
        std::cout << layer_properties.layerName << std::endl;

    //check that we have all needed validation layers
    for(const char* needed_layer : _needed_validation_layers)
    {
        const bool has_needed_layer = std::any_of(begin(available_layers), end(available_layers),
        [&needed_layer](const VkLayerProperties& layer)
        {
            return strcmp(needed_layer, layer.layerName) == 0;
        });

        if(!has_needed_layer)
            return false;
    }

    return true;

}
