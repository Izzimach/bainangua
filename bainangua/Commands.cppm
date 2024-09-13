module;

#include "bainangua.hpp"

#include <functional>

export module Commands;

import OuterBoilerplate;

namespace bainangua {

export inline vk::CommandPoolCreateInfo defaultCommandPool(const OuterBoilerplateState& s)
{
	return vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);
}

export inline vk::CommandBufferAllocateInfo defaultCommandBuffer(vk::CommandPool p)
{
	return vk::CommandBufferAllocateInfo(p, vk::CommandBufferLevel::ePrimary, 1);
}


export void withCommandPool(const OuterBoilerplateState& s, const vk::CommandPoolCreateInfo& info, std::function<void(vk::CommandPool)> wrapped)
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

export void withCommandBuffers(const OuterBoilerplateState& s, const vk::CommandBufferAllocateInfo& info, std::function<void(std::vector<vk::CommandBuffer> &)> wrapped)
{
	std::vector<vk::CommandBuffer> buffers = s.vkDevice.allocateCommandBuffers(info);
	try {
		wrapped(buffers);
	}
	catch (const std::exception& e) {
		s.vkDevice.freeCommandBuffers(info.commandPool, buffers);
		throw e;
	}
	s.vkDevice.freeCommandBuffers(info.commandPool, buffers);
}

export void withCommandBuffer(const OuterBoilerplateState& s,vk::CommandPool pool, std::function<void(vk::CommandBuffer)> wrapped)
{
	std::vector<vk::CommandBuffer> buffers = s.vkDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1));
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