#include "vk_mesh.h"

Mesh::Mesh(VkPhysicalDevice p_device, VkDevice l_device, std::vector<Vertex> &vertices):
	_vertex_count(static_cast<uint32_t>(vertices.size())),
	_physical_device(p_device),
	_logical_device(l_device)
{
	create_vertex_buffer(vertices);
}

void Mesh::create_vertex_buffer(std::vector<Vertex> &vertices)
{
	//1
	//just layout of buffer
	VkBufferCreateInfo buffer_create_info
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(Vertex) * vertices.size(),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		//1 thing will use this buffer
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VkResult res = vkCreateBuffer(_logical_device, &buffer_create_info, nullptr, &_vertex_buffer);
	if(res != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vertex buffer!");
	}

	//2
	VkMemoryRequirements memory_requirements;
	vkGetBufferMemoryRequirements(_logical_device, _vertex_buffer, &memory_requirements);

	auto findMemoryTypeIndex = [&](uint32_t allowed_types/*defined by buffer*/, VkMemoryPropertyFlags properties/*defined by ourselfs*/)
	{
		VkPhysicalDeviceMemoryProperties physical_properties;
		//Actual properties of memory sections of my GPU
		vkGetPhysicalDeviceMemoryProperties(_physical_device, &physical_properties);

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

	uint32_t property_flags =
		// chunk of memory is visible to the CPU (host)
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		// after being mapped, data is placed straight into the buffer (without it we need manually flush)
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	VkMemoryAllocateInfo alloc_info
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memory_requirements.size,
		.memoryTypeIndex = findMemoryTypeIndex(memory_requirements.memoryTypeBits, property_flags)
	};

	//3
	res = vkAllocateMemory(_logical_device, &alloc_info, nullptr, &_vertex_buffer_memory);
	if(res != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate vertex buffer memory!");
	}

	//4
	//Bind allocated data to the buffer
	vkBindBufferMemory(_logical_device, _vertex_buffer, _vertex_buffer_memory, 0);

	//5
	//Map vertex data to the vertex buffer
	void *data = nullptr;
	vkMapMemory(_logical_device, _vertex_buffer_memory, 0, buffer_create_info.size, 0, &data);
	//data is not points to the buffer memory (on GPU?)
	std::memcpy(data, vertices.data(), (size_t)buffer_create_info.size);
	vkUnmapMemory(_logical_device, _vertex_buffer_memory);
}
