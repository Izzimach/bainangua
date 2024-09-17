module;

#include "bainangua.hpp"

#include <functional>

export module Commands;

import VulkanContext;

namespace bainangua {

export inline vk::CommandPoolCreateInfo defaultCommandPool(const VulkanContext& s)
{
	return vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);
}

export inline vk::CommandBufferAllocateInfo defaultCommandBuffer(vk::CommandPool p)
{
	return vk::CommandBufferAllocateInfo(p, vk::CommandBufferLevel::ePrimary, 1);
}

export void withCommandPool(const VulkanContext& s, const vk::CommandPoolCreateInfo& info, std::function<void(vk::CommandPool)> wrapped)
{
	vk::CommandPool pool = s.vkDevice.createCommandPool(info);
	try {
		wrapped(pool);
	}
	catch (std::exception &e) {
		s.vkDevice.destroyCommandPool(pool);
		throw e;
	}
	s.vkDevice.destroyCommandPool(pool);
}

export void withCommandBuffers(const VulkanContext& s, const vk::CommandBufferAllocateInfo& info, std::function<void(std::pmr::vector<vk::CommandBuffer> &)> wrapped)
{
	std::pmr::vector<vk::CommandBuffer> buffers = s.vkDevice.allocateCommandBuffers<std::pmr::polymorphic_allocator<vk::CommandBuffer>>(info);
	try {
		wrapped(buffers);
	}
	catch (const std::exception& e) {
		s.vkDevice.freeCommandBuffers(info.commandPool, buffers);
		throw e;
	}
	s.vkDevice.freeCommandBuffers(info.commandPool, buffers);
}

export void withCommandBuffer(const VulkanContext& s, vk::CommandPool pool, std::function<void(vk::CommandBuffer)> wrapped)
{
	std::pmr::vector<vk::CommandBuffer> buffers = s.vkDevice.allocateCommandBuffers<std::pmr::polymorphic_allocator<vk::CommandBuffer>>(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1));
	try {
		wrapped(buffers[0]);
	}
	catch (std::exception& e) {
		s.vkDevice.freeCommandBuffers(pool, buffers);
		throw e;
	}
	s.vkDevice.freeCommandBuffers(pool, buffers);
}

}