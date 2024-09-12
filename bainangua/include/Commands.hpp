#pragma once

#include "bainangua.hpp"
#include "OuterBoilerplate.hpp"

#include <functional>

namespace bainangua {

void withCommandPool(const OuterBoilerplateState& s, const vk::CommandPoolCreateInfo& info, std::function<void(vk::CommandPool) > wrapped);

void withCommandBuffers(const OuterBoilerplateState& s, const vk::CommandBufferAllocateInfo& info, std::function<void(std::vector<vk::CommandBuffer>)> wrapped);

vk::CommandPoolCreateInfo defaultCommandPool(const OuterBoilerplateState& s);

vk::CommandBufferAllocateInfo defaultCommandBuffer(vk::CommandPool p);


inline vk::CommandPoolCreateInfo defaultCommandPool(const OuterBoilerplateState& s)
{
	return vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);
}

inline vk::CommandBufferAllocateInfo defaultCommandBuffer(vk::CommandPool p)
{
	return vk::CommandBufferAllocateInfo(p, vk::CommandBufferLevel::ePrimary, 1);
}


}