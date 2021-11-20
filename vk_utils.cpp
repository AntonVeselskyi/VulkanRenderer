#include <iostream>
#include "vk_utils.h"

void create_buffer(const VkPhysicalDevice p_device, VkDevice l_device, VkDeviceSize buffer_size,
                   VkBufferUsageFlags buffer_usage_falgs, VkMemoryPropertyFlags buffer_property_falgs,
                   VkBuffer *vertex_buffer, VkDeviceMemory *vertex_buffer_memory)
{
    //1
    //just layout of buffer
    VkBufferCreateInfo buffer_create_info
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = buffer_size,
        .usage = buffer_usage_falgs,
        //1 thing will use this buffer
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkResult res = vkCreateBuffer(l_device, &buffer_create_info, nullptr, vertex_buffer);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create vertex buffer!");
    }

    //2
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(l_device, *vertex_buffer, &memory_requirements);

    auto findMemoryTypeIndex = [&](uint32_t allowed_types/*defined by buffer*/, VkMemoryPropertyFlags properties/*defined by ourselfs*/)
    {
        VkPhysicalDeviceMemoryProperties physical_properties;
        //Actual properties of memory sections of my GPU
        vkGetPhysicalDeviceMemoryProperties(p_device, &physical_properties);

        for(uint32_t i = 0; i < physical_properties.memoryTypeCount; ++i)
        {
            const bool is_type_allowed = allowed_types & (1 << i);
            //all properties flags must match
            const bool is_type_flags_match = (physical_properties.memoryTypes[i].propertyFlags & properties) == properties;

            //return valid type
            if(is_type_allowed && is_type_flags_match)
                return i;
        }
    };

    VkMemoryAllocateInfo alloc_info
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = findMemoryTypeIndex(memory_requirements.memoryTypeBits, buffer_property_falgs)
    };

    //3
    res = vkAllocateMemory(l_device, &alloc_info, nullptr, vertex_buffer_memory);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate vertex buffer memory!");
    }

    //4
    //Bind allocated data to the buffer
    vkBindBufferMemory(l_device, *vertex_buffer, *vertex_buffer_memory, 0);
}

void copy_buffer(VkDevice l_device, VkQueue transfer_queue, VkCommandPool transfer_command_pool,
                 VkBuffer src, VkBuffer dst, VkDeviceSize buffer_size)
{
    //buffer to hold transfer commands
    VkCommandBuffer transfer_command_buffer;
    VkCommandBufferAllocateInfo  transfer_command_buffer_alloc_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = transfer_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(l_device, &transfer_command_buffer_alloc_info, &transfer_command_buffer);

    VkCommandBufferBeginInfo begin_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        //one time usage of this buffer
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    //Start recording transfer commands
    vkBeginCommandBuffer(transfer_command_buffer, &begin_info);
    
    //Details of what to copy where
    VkBufferCopy buffer_copy_region
    {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = buffer_size
    };
    vkCmdCopyBuffer(transfer_command_buffer, src, dst, 1, &buffer_copy_region);

    vkEndCommandBuffer(transfer_command_buffer);

    VkSubmitInfo submit_info
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &transfer_command_buffer
    };
    vkQueueSubmit(transfer_queue, 1, &submit_info, VK_NULL_HANDLE);

    vkQueueWaitIdle(transfer_queue);
    vkFreeCommandBuffers(l_device, transfer_command_pool, 1, &transfer_command_buffer);
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
        if(qfamily.queueCount > 0 and presentation_family_support)
            indecies.presentation_family = indx;

        if(indecies.is_valid())
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
        return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    for(const VkSurfaceFormatKHR &sf : formats)
    {
        if(
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

        uint32_t height_to_set = std::min(capabilities.maxImageExtent.height, (uint32_t)height);
        width_to_set = std::max(capabilities.minImageExtent.height, (uint32_t)height);

        return {.width = width_to_set, .height = height_to_set};
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
    if(presentation_count)
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

VkShaderModule create_shader_module(VkDevice logical_device, std::vector<char> &shader_code)
{
    VkShaderModuleCreateInfo create_info
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader_code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(shader_code.data())
    };

    VkShaderModule shader_module;
    VkResult res = vkCreateShaderModule(logical_device, &create_info, nullptr, &shader_module);
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shader_module;
}
