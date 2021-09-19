#include "vulkan_renderer.h"

#include <algorithm>
#include <cstring>
#include <set>
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
        vkGetDeviceQueue(_main_device.logical_device, _main_device.queue_indecies.graphics_family, 0, &_graphics_queue);
        vkGetDeviceQueue(_main_device.logical_device, _main_device.queue_indecies.presentation_family, 0, &_presentation_queue);
        //so far we checked that device supports presenting to our surface and created the queue that allows us to do that
        
        create_swapchain();
    }
    catch (std::runtime_error &e)
    {
        std::cerr << "fuck error: " <<  e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void VulkanRenderer::cleanup()
{
    //images are destroyed by the swapchain, but image views clean up is up to us
    for(auto image : _swapchain_images)
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
            _main_device.queue_indecies =  get_queue_families_for_device(dev, _surface);
            break;
        }
    
}

bool VulkanRenderer::check_device_suitable(const VkPhysicalDevice &device)
{
    /*
    //Info about device itself
    VkPhysicalDeviceProperties device_props;
    vkGetPhysicalDeviceProperties(device, &device_props);

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
    std::set<uint32_t> queue_family_indices {_main_device.queue_indecies.graphics_family, _main_device.queue_indecies.presentation_family};
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

QueueFamilyIndices get_queue_families_for_device(const VkPhysicalDevice &device, const VkSurfaceKHR &surface)
{
    QueueFamilyIndices indecies;

    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> qfp(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, qfp.data());
    
    //check that at least one queue family has at least 1 type of needed quque
    for(uint32_t indx = 0; indx <= count; indx++)
    {
        const auto &qfamily = qfp[indx];
        if(qfamily.queueCount > 0 and qfamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indecies.graphics_family = indx;
        }

        //check if a queue family(of this device) supports a surface
        VkBool32 presentation_family_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, indx, surface, &presentation_family_support);
        //chech if queue is presentation type (can be both graphics and pressentation)
        if (qfamily.queueCount > 0 and presentation_family_support)
            indecies.presentation_family = indx;

        if (indecies.is_valid())
        {
            break;
        }
    }

    return indecies;
}

//Best format is subjective, but I took:
//Format: VK_Format_R8G8B8A8_UNFORM
//ColorSpace: VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
//can be reworked to be more sophisticated
VkSurfaceFormatKHR choose_best_surface_format(const std::vector<VkSurfaceFormatKHR> &formats)
{
    //not obvious but VK_FORMAT_UNDEFINED means all formats are supported :/
    if(formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED)
    {
        return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }

    for(const VkSurfaceFormatKHR& sf : formats)
    {
        if (
            (sf.format == VK_FORMAT_R8G8B8A8_UNORM || sf.format == VK_FORMAT_B8G8R8A8_UNORM)
            && sf.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            )
        {
            return sf;
        }
    }
    return formats.front();
}

VkPresentModeKHR choose_best_presentation_mode(const std::vector<VkPresentModeKHR> &modes)
{
    auto has_mailbox = std::find(begin(modes), end(modes), VK_PRESENT_MODE_MAILBOX_KHR);
    
    //FIFO is always available
    return has_mailbox != end(modes) ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D choose_best_swap_extent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window)
{
    //find what size swapchain could be based on capabilities of the surface
    //surface took info from the actual OS window
    
    
    if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;
    else //if set to max
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        //bound width, height to the capabilities
        uint32_t width_to_set = std::min(capabilities.maxImageExtent.width, (uint32_t)width);
        width_to_set = std::max(capabilities.minImageExtent.width, (uint32_t)width);

        uint32_t height_to_set = std::min(capabilities.maxImageExtent.height,(uint32_t)height);
        width_to_set = std::max(capabilities.minImageExtent.height, (uint32_t)height);

        return {.width=width_to_set, .height=height_to_set};
    }

}

SwapchainCreationDetails get_swapchain_details_for_device(const VkPhysicalDevice &device, const VkSurfaceKHR &surface)
{
    SwapchainCreationDetails details;

    //Get the surface capabilities for the given surface for the given physical device
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.surface_capabilities);
    
    uint32_t formats_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formats_count, nullptr);
    if(formats_count)
    {
        details.surface_formats.resize(formats_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formats_count, details.surface_formats.data());
    }
    std::cout << "Physical device supports: " << formats_count << " surface formats\n";

    uint32_t presentation_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentation_count, nullptr);
    if (presentation_count)
    {
        details.presentation_modes.resize(presentation_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentation_count, details.presentation_modes.data());
    }
    return details;
}

VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        //SWIZZLE_IDENTITY -- keep it`s own identity
        //r points to r and so on
        //.r=VK_COMPONENT_SWIZZLE_G -- would make R always take the value of G
        .components = 
        {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        //allow the view to view only a part of an image
        //(e.g. )
        .subresourceRange =
        {
            //Which aspect of the image to view
            .aspectMask = aspect_flags,
            .baseMipLevel = 0, //Start MIPMAP level to view from
            .levelCount = 1, // NUM of mipmap levels to view from
            .baseArrayLayer = 0, //Start array level to view from 
            .layerCount = 1,
        }

    };

    //create and return handle to image view separatly
    VkImageView image_view;
    if(vkCreateImageView(device, &create_info, nullptr, &image_view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create an image view");

    return image_view;
}
