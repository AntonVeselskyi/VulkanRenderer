#include "vk_mesh.h"
#include "vk_utils.h"

Mesh::Mesh(VkPhysicalDevice p_device, VkDevice l_device,
		   VkQueue transfer_queue, VkCommandPool command_pool,
		   std::vector<Vertex> &vertices, std::vector<uint32_t> indices):
	_vertex_count(static_cast<uint32_t>(vertices.size())),
	_index_count(static_cast<uint32_t>(indices.size())),
	_physical_device(p_device),
	_logical_device(l_device)
{
	create_vertex_buffer(vertices, transfer_queue, command_pool);
	create_index_buffer(indices, transfer_queue, command_pool);

	_ubo_model.model = glm::mat4(1.f);
}

void Mesh::create_vertex_buffer(std::vector<Vertex> &vertices, VkQueue transfer_queue, VkCommandPool command_pool)
{
	size_t buffer_size = sizeof(vertices) * vertices.size();

	//Create a temporary staging buffer
	//will use it to transfer vertex data to the GPU (DEVICE_LOCAL) memory
	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	uint32_t staging_property_flags =
		// chunk of memory is visible to the CPU host
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		// after being mapped, data is placed straight into the buffer (without it we need manually flush)
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	create_buffer(_physical_device, _logical_device, buffer_size,
				  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_property_flags,
				  &staging_buffer, &staging_buffer_memory);

	//Map vertex data to the staging buffer
	void *data = nullptr;
	vkMapMemory(_logical_device, staging_buffer_memory, 0, buffer_size, 0, &data);
	//data is now points to the buffer memory
	//put data into staging (CPU visible part of GPU)
	std::memcpy(data, vertices.data(), buffer_size);
	vkUnmapMemory(_logical_device, staging_buffer_memory);


	//Create actual buffer that will used and seen only by GPU
	create_buffer(_physical_device, _logical_device, buffer_size,
			      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //memory visible only to the GPU 
				  &_vertex_buffer, &_vertex_buffer_memory);

	//Transfer data with transfer queue
	copy_buffer(_logical_device, transfer_queue, command_pool, staging_buffer, _vertex_buffer, buffer_size);

	//Staging is not needed after copy
	vkDestroyBuffer(_logical_device, staging_buffer, nullptr);
	vkFreeMemory(_logical_device, staging_buffer_memory, nullptr);
}

void Mesh::create_index_buffer(std::vector<uint32_t> &indices, VkQueue transfer_queue, VkCommandPool command_pool)
{
	//staging part is same as for vertex buffer
	size_t buffer_size = sizeof(uint32_t) * indices.size();

	VkBuffer staging_buffer;
	VkDeviceMemory staging_buffer_memory;

	uint32_t staging_property_flags =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	create_buffer(_physical_device, _logical_device, buffer_size,
				  VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_property_flags,
				  &staging_buffer, &staging_buffer_memory);

	void *data = nullptr;
	vkMapMemory(_logical_device, staging_buffer_memory, 0, buffer_size, 0, &data);
	std::memcpy(data, indices.data(), buffer_size);
	vkUnmapMemory(_logical_device, staging_buffer_memory);


	//Create actual INDEX buffer that will used and seen only by GPU
	create_buffer(_physical_device, _logical_device, buffer_size,
				  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, //memory visible only to the GPU 
				  &_index_buffer, &_index_buffer_memory);

	copy_buffer(_logical_device, transfer_queue, command_pool, staging_buffer, _index_buffer, buffer_size);

	vkDestroyBuffer(_logical_device, staging_buffer, nullptr);
	vkFreeMemory(_logical_device, staging_buffer_memory, nullptr);
}
