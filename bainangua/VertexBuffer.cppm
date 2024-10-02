module;

#include "bainangua.hpp"
#include "blaze/Blaze.h"
#include "vk_mem_alloc.h"

#include <expected.hpp>
#include <reflect>

export module VertexBuffer;

namespace bainangua {

export
struct VTVertex {
	blaze::StaticVector<float, 2UL> pos2;
	blaze::StaticVector<float, 3UL> color;
};

export
auto getVTVertexBindingAndAttributes() -> std::tuple<vk::VertexInputBindingDescription, std::vector<vk::VertexInputAttributeDescription>> {
	vk::VertexInputBindingDescription bindingDescription(0, sizeof(VTVertex), vk::VertexInputRate::eVertex);
	std::vector<vk::VertexInputAttributeDescription> attributes{
		vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(VTVertex, pos2)),
		vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(VTVertex, color))
	};

	return {bindingDescription, attributes};
}

export std::vector<VTVertex> staticVertices = {
	{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, 0.5f},  {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}
};

export
template <typename V>
auto createVertexBuffer(VmaAllocator allocator, const std::vector<V>& vertexData) -> tl::expected<std::tuple<vk::Buffer, VmaAllocation>, bainangua::bng_errorobject> {
	size_t dataSize = sizeof(vertexData[0]) * vertexData.size();
	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = dataSize,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo vmaAllocateInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = VK_NULL_HANDLE,
		.pUserData = nullptr,
		.priority = 0.0f
	};
	VkBuffer buffer;
	VmaAllocation allocation;
	auto vkResult = vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaAllocateInfo, &buffer, &allocation, nullptr);
	if (vkResult != VK_SUCCESS) {
		return tl::make_unexpected("vmaCreateBuffer failed");
	}

	// dump data into buffer
	void* data;
	auto mapResult = vmaMapMemory(allocator, allocation, &data);
	if (mapResult != VK_SUCCESS) {
		vmaDestroyBuffer(allocator, buffer, allocation);
		return tl::make_unexpected("vmaMapMemory failed");
	}
	memcpy(data, vertexData.data(), dataSize);
	vmaUnmapMemory(allocator, allocation);

	return std::make_tuple(buffer, allocation);
}

export
void destroyVertexBuffer(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation alloc) {
	vmaDestroyBuffer(allocator, buffer, alloc);
}

}