module;

#include "bainangua.hpp"
#include "blaze/Blaze.h"
#include "vk_mem_alloc.h"

#include <expected.hpp>
#include <reflect>

export module VertexBuffer;

import VulkanContext;

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
template <typename V>
auto createGPUVertexBuffer(VmaAllocator allocator, const VulkanContext& s, vk::CommandPool pool, const std::vector<V>& vertexData) -> tl::expected<std::tuple<vk::Buffer, VmaAllocation>, bainangua::bng_errorobject> {
	// create two buffers, one on GPU and one host-visible

	auto hostVisibleBuffer = createVertexBuffer(allocator, vertexData);
	if (!hostVisibleBuffer.has_value()) {
		return tl::make_unexpected("createGPUVertexBuffer: cannot create host-visible buffer");
	}
	auto [hostBuffer, hostAllocation] = hostVisibleBuffer.value();

	// GPU buffer
	size_t dataSize = sizeof(vertexData[0]) * vertexData.size();
	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = dataSize,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo vmaAllocateInfo{
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE ,
		.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = VK_NULL_HANDLE,
		.pUserData = nullptr,
		.priority = 0.0f
	};

	VkBuffer GPUbuffer;
	VmaAllocation GPUallocation;
	auto vkResult = vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaAllocateInfo, &GPUbuffer, &GPUallocation, nullptr);
	if (vkResult != VK_SUCCESS) {
		vmaDestroyBuffer(allocator, hostBuffer, hostAllocation);
		return tl::make_unexpected("createGPUVertexBuffer: vmaCreateBuffer failed");
	}

	std::pmr::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers<std::pmr::polymorphic_allocator<vk::CommandBuffer>>(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1));

	
	vk::CommandBuffer commandBuffer = commandBuffers[0];

	vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	vk::Result commandBeginResult = commandBuffer.begin(&beginInfo);
	if (commandBeginResult != vk::Result::eSuccess) {
		s.vkDevice.freeCommandBuffers(pool, commandBuffers);
		vmaDestroyBuffer(allocator, hostBuffer, hostAllocation);
		vmaDestroyBuffer(allocator, GPUbuffer, GPUallocation);
		return tl::make_unexpected("createGPUVertexBuffer: begin command buffer failed");
	}

	vk::BufferCopy copyRegion(0,0,dataSize);
	commandBuffer.copyBuffer(hostBuffer, GPUbuffer, 1, &copyRegion);
	
	commandBuffer.end();
	
	vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &commandBuffer, 0, nullptr);

	s.graphicsQueue.submit(submitInfo);
	s.graphicsQueue.waitIdle();

	s.vkDevice.freeCommandBuffers(pool, commandBuffers);

	vmaDestroyBuffer(allocator, hostBuffer, hostAllocation);

	return std::make_tuple(GPUbuffer, GPUallocation);
}

export
void destroyVertexBuffer(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation alloc) {
	vmaDestroyBuffer(allocator, buffer, alloc);
}

}