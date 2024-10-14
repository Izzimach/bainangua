module;

#include "bainangua.hpp"
#include "expected.hpp"
#include "RowType.hpp"
#include "vk_mem_alloc.h"

#include <glm/glm.hpp>
#include <reflect>

export module VertexBuffer;

import VulkanContext;

namespace bainangua {

export
struct VTVertex {
	glm::vec2 pos2;
	glm::vec3 color;
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

export const std::vector<VTVertex> staticVertices = {
	{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, 0.5f},  {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}
};

export const std::vector<VTVertex> indexedStaticVertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

export const std::vector<uint16_t> staticIndices = {
	0, 1, 2, 2, 3, 0
};

export
template <typename V>
auto createVertexBuffer(VmaAllocator allocator, const std::vector<V>& vertexData) -> tl::expected<std::tuple<vk::Buffer, VmaAllocation>, bainangua::bng_errorobject> {
	size_t dataSize = sizeof(vertexData[0]) * vertexData.size();
	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = dataSize,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo vmaAllocateInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = 0,
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
auto copyBuffer(const VulkanContext& s, vk::CommandPool pool, VkBuffer source, VkBuffer dest, size_t dataSize) -> vk::Result {
	std::pmr::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers<std::pmr::polymorphic_allocator<vk::CommandBuffer>>(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1));

	vk::CommandBuffer commandBuffer = commandBuffers[0];

	vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	vk::Result commandBeginResult = commandBuffer.begin(&beginInfo);
	if (commandBeginResult != vk::Result::eSuccess) {
		s.vkDevice.freeCommandBuffers(pool, commandBuffers);
		return commandBeginResult;
	}

	vk::BufferCopy copyRegion(0, 0, dataSize);
	commandBuffer.copyBuffer(source, dest, 1, &copyRegion);
	commandBuffer.end();

	vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &commandBuffer, 0, nullptr);

	s.graphicsQueue.submit(submitInfo);
	s.graphicsQueue.waitIdle();

	s.vkDevice.freeCommandBuffers(pool, commandBuffers);

	return vk::Result::eSuccess;
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
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo vmaAllocateInfo{
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = 0,
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

	vk::Result copyResult = copyBuffer(s, pool, hostBuffer, GPUbuffer, dataSize);
	if (copyResult != vk::Result::eSuccess) {
		vmaDestroyBuffer(allocator, hostBuffer, hostAllocation);
		vmaDestroyBuffer(allocator, GPUbuffer, GPUallocation);
		return tl::make_unexpected("createGPUVertexBuffer: begin command buffer failed");
	}

	vmaDestroyBuffer(allocator, hostBuffer, hostAllocation);

	return std::make_tuple(GPUbuffer, GPUallocation);
}

export
template <typename Index>
auto createGPUIndexBuffer(VmaAllocator allocator, const VulkanContext& s, vk::CommandPool pool, const std::vector<Index> &indices) -> tl::expected<std::tuple<vk::Buffer, VmaAllocation>, bainangua::bng_errorobject> {
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo vmaAllocateInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = 0,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = VK_NULL_HANDLE,
		.pUserData = nullptr,
		.priority = 0.0f
	};
	VkBuffer stagingBuffer;
	VmaAllocation stagingAllocation;
	auto vkResult = vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaAllocateInfo, &stagingBuffer, &stagingAllocation, nullptr);
	if (vkResult != VK_SUCCESS) {
		return tl::make_unexpected("createGPUIndexBuffer: vmaCreateBuffer for staging buffer failed");
	}
	VmaAllocationInfo StagingInfo;
	vmaGetAllocationInfo(allocator, stagingAllocation, &StagingInfo);
	if (StagingInfo.pMappedData == nullptr) {
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		return tl::make_unexpected("createGPUIndexBuffer: vmaCreateBuffer for staging buffer not mapped");
	}

	VkBufferCreateInfo GPUbufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo GPUvmaAllocateInfo{
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = 0,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = VK_NULL_HANDLE,
		.pUserData = nullptr,
		.priority = 0.0f
	};

	VkBuffer GPUbuffer;
	VmaAllocation GPUallocation;
	auto GPUResult = vmaCreateBuffer(allocator, &GPUbufferCreateInfo, &GPUvmaAllocateInfo, &GPUbuffer, &GPUallocation, nullptr);
	if (GPUResult != VK_SUCCESS) {
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		return tl::make_unexpected("createGPUIndexBuffer: vmaCreateBuffer for GPU failed");
	}

	memcpy(StagingInfo.pMappedData, indices.data(), (size_t)bufferSize);

	copyBuffer(s, pool, stagingBuffer, GPUbuffer, bufferSize);

	vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

	return std::make_tuple(GPUbuffer, GPUallocation);
}

export
void destroyVertexBuffer(VmaAllocator allocator, vk::Buffer buffer, VmaAllocation alloc) {
	vmaDestroyBuffer(allocator, buffer, alloc);
}

export
void destroyVertexBuffer(VmaAllocator allocator, std::tuple<vk::Buffer, VmaAllocation> vertexData) {
	vmaDestroyBuffer(allocator, std::get<0>(vertexData), std::get<1>(vertexData));
}

export struct GPUVertexBufferStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));

		auto vertexResult = bainangua::createGPUVertexBuffer(context.vmaAllocator, context, commandPool, bainangua::staticVertices);
		if (!vertexResult.has_value()) {
			return tl::make_unexpected<bng_errorobject>("GPUVertexBufferStage: could not create GPU vertex buffer");
		}

		auto vertexValue = vertexResult.value();
		auto rWithVertexBuffer = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("vertexBuffer"), vertexValue));
		auto result = f.applyRow(rWithVertexBuffer);

		bainangua::destroyVertexBuffer(context.vmaAllocator, vertexValue);

		return result;
	}
};
export struct GPUIndexedVertexBufferStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));

		auto vertexResult = bainangua::createGPUVertexBuffer(context.vmaAllocator, context, commandPool, bainangua::indexedStaticVertices);
		if (!vertexResult.has_value()) {
			return tl::make_unexpected<bng_errorobject>("GPUVertexBufferStage: could not create GPU vertex buffer");
		}

		auto vertexValue = vertexResult.value();
		auto rWithVertexBuffer = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("indexedVertexBuffer"), vertexValue));
		auto result = f.applyRow(rWithVertexBuffer);

		bainangua::destroyVertexBuffer(context.vmaAllocator, vertexValue);

		return result;
	}
};

export struct GPUIndexBufferStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));

		auto indexResult = bainangua::createGPUIndexBuffer(context.vmaAllocator, context, commandPool, bainangua::staticIndices);
		if (!indexResult.has_value()) {
			return tl::make_unexpected<bng_errorobject>("GPUIndexBufferStage: could not create GPU index buffer");
		}

		auto indexValue = indexResult.value();
		auto rWithIndexBuffer = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("indexBuffer"), indexValue));
		auto result = f.applyRow(rWithIndexBuffer);

		bainangua::destroyVertexBuffer(context.vmaAllocator, indexValue);

		return result;
	}
};

}