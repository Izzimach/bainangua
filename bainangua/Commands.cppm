module;

#include "bainangua.hpp"
#include "RowType.hpp"

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


export struct SimpleGraphicsCommandPoolStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));

		vk::CommandPool pool = context.vkDevice.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, context.graphicsQueueFamilyIndex));

		auto rWithPool = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("commandPool"), pool));
		auto result = f.applyRow(rWithPool);

		context.vkDevice.destroyCommandPool(pool);

		return result;
	}
};

export struct PrimaryGraphicsCommandBuffersStage {
	PrimaryGraphicsCommandBuffersStage(uint32_t count) : count_(count) {}
		
	uint32_t count_;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));

		std::pmr::vector<vk::CommandBuffer> commandBuffers = context.vkDevice.allocateCommandBuffers<std::pmr::polymorphic_allocator<vk::CommandBuffer>>(vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, count_));

		auto rWithBuffers = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("commandBuffers"), commandBuffers));
		auto result = f.applyRow(rWithBuffers);

		// don't need to destroy command buffers
		return result;
	}
};

}