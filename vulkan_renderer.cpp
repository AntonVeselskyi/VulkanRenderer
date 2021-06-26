#include "vulkan_renderer.h"

#include <algorithm>
#include <cstring>

#include "io_utils.h"

int VulkanRenderer::init(GLFWwindow *new_window)
{
    _window = new_window;

    try
    {
        create_instance();
        get_physical_device();
        create_logical_device();

        //get our graphics queue (VkQueue) from device
        vkGetDeviceQueue(main_device.logical_device, main_device.queue_indecies.graphics_family, 0, &graphics_queue);
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
    vkDestroyDevice(main_device.logical_device, nullptr);
    vkDestroyInstance(instance, nullptr);
}


void VulkanRenderer::get_physical_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    //are there any devices
    if(!device_count)
        throw std::runtime_error("No GPU compatable with Vulkan found");

    std::vector<VkPhysicalDevice> device_list(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, device_list.data());

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
            main_device.physical_device = dev;
            //after device is chosen, lets save queue family index
            main_device.queue_indecies =  get_queue_families_for_device(dev);
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
   return get_queue_families_for_device(device).is_valid();
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

void VulkanRenderer::create_logical_device()
{
    //queues logical device need to create
    float priority = 1.f; // 1 - highest priority
    VkDeviceQueueCreateInfo queue_create_info
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = main_device.queue_indecies.graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &priority //vulkan needs to know how to handle different queues
    };
    
    VkPhysicalDeviceFeatures pd_features{};
    //Device === Logical Device
    VkDeviceCreateInfo device_create_info
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1, 
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
        //.enabledLayerCount depricated, handled by instance
        .pEnabledFeatures = &pd_features
    };

    VkResult result = vkCreateDevice(main_device.physical_device, &device_create_info, nullptr, &main_device.logical_device);
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

    uint32_t validation_layers_count = (uint32_t)(enable_validation_layers ? needed_validation_layers.size() : 0);
    auto *validation_layers_names = (enable_validation_layers ? needed_validation_layers.data() : nullptr);

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
    VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
    if(result != VK_SUCCESS)
        throw std::runtime_error("Failed to create a Vulkan Instance");
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
    for(const char* needed_layer : needed_validation_layers)
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

QueueFamilyIndeces get_queue_families_for_device(const VkPhysicalDevice &device)
{
    QueueFamilyIndeces indecies;

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
            break;
        }
    }

    return indecies;
}
