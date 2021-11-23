#pragma once

#include "vk_utils.h"

struct UBOModel
{
	//where object is in the world
	glm::mat4 model;
};

class Mesh
{
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice p_device, VkDevice l_device,
		 VkQueue transfer_queue, VkCommandPool command_pool,
		 std::vector<Vertex> &vertices, std::vector<uint32_t> indices);
	void destroy_buffers()
	{
		vkDestroyBuffer(_logical_device, _vertex_buffer, nullptr);
		vkFreeMemory(_logical_device, _vertex_buffer_memory, nullptr);
		vkDestroyBuffer(_logical_device, _index_buffer, nullptr);
		vkFreeMemory(_logical_device, _index_buffer_memory, nullptr);
	}

	uint32_t get_vertex_count() { return _vertex_count; }
	VkBuffer get_vertex_buffer() { return _vertex_buffer; }
	uint32_t get_index_count() { return _index_count; }
	VkBuffer get_index_buffer() { return _index_buffer; }

	void set_model(glm::mat4 m) { _ubo_model.model = m; }
	UBOModel get_model() { return _ubo_model; }


private:
	//each mesh holds its position in the world
	UBOModel _ubo_model;

	VkPhysicalDevice _physical_device;
	VkDevice _logical_device;

	uint32_t _vertex_count;
	VkBuffer _vertex_buffer;
	VkDeviceMemory _vertex_buffer_memory;
	uint32_t _index_count;
	VkBuffer _index_buffer;
	VkDeviceMemory _index_buffer_memory;

	void create_vertex_buffer(std::vector<Vertex> &vertices, VkQueue transfer_queue, VkCommandPool command_pool);
	void create_index_buffer(std::vector<uint32_t> &indices, VkQueue transfer_queue, VkCommandPool command_pool);
};

