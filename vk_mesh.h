#pragma once

#include "vk_utils.h"

class Mesh
{
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice , VkDevice new_device, std::vector<Vertex> &vertices);
	void destroy_vertex_buffer()
	{
		vkDestroyBuffer(_logical_device, _vertex_buffer, nullptr);
		vkFreeMemory(_logical_device, _vertex_buffer_memory, nullptr);
	}

	uint32_t get_vertex_count() { return _vertex_count; }
	VkBuffer get_vertex_buffer() { return _vertex_buffer; }

private:
	VkPhysicalDevice _physical_device;
	VkDevice _logical_device;

	uint32_t _vertex_count;
	VkBuffer _vertex_buffer;
	VkDeviceMemory _vertex_buffer_memory;

	void create_vertex_buffer(std::vector<Vertex> &vertices);
};

