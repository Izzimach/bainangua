module;

#include "bainangua.hpp"
#include "RowType.hpp"

#include <functional>

export module Commands;

import VulkanContext;

namespace bainangua {

export inline vk::CommandPoolCreateInfo defaultCommandPool(uint32_t graphicsQueueFamilyIndex)
{
	return vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsQueueFamilyIndex);
}

export inline vk::CommandBufferAllocateInfo defaultCommandBuffer(vk::CommandPool p)
{
	return vk::CommandBufferAllocateInfo(p, vk::CommandBufferLevel::ePrimary, 1);
}

export void withCommandPool(vk::Device device, const vk::CommandPoolCreateInfo& info, std::function<void(vk::CommandPool)> wrapped)
{
	vk::CommandPool pool = device.createCommandPool(info);
	try {
		wrapped(pool);
	}
	catch (std::exception &e) {
		device.destroyCommandPool(pool);
		throw e;
	}
	device.destroyCommandPool(pool);
}

export void withCommandBuffers(vk::Device device, const vk::CommandBufferAllocateInfo& info, std::function<void(std::vector<vk::CommandBuffer> &)> wrapped)
{
	std::vector<vk::CommandBuffer> buffers = device.allocateCommandBuffers(info);
	try {
		wrapped(buffers);
	}
	catch (const std::exception& e) {
		device.freeCommandBuffers(info.commandPool, buffers);
		throw e;
	}
	device.freeCommandBuffers(info.commandPool, buffers);
}

export void withCommandBuffer(vk::Device device, vk::CommandPool pool, std::function<void(vk::CommandBuffer)> wrapped)
{
	std::vector<vk::CommandBuffer> buffers = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1));
	try {
		wrapped(buffers[0]);
	}
	catch (std::exception& e) {
		device.freeCommandBuffers(pool, buffers);
		throw e;
	}
	device.freeCommandBuffers(pool, buffers);
}

export auto submitCommand(vk::Device device, vk::Queue graphicsQueue, vk::CommandPool pool, std::function<vk::Result(vk::CommandBuffer)> commands) -> vk::Result {
	std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, 1));

	vk::CommandBuffer commandBuffer = commandBuffers[0];

	vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	vk::Result commandBeginResult = commandBuffer.begin(&beginInfo);
	if (commandBeginResult != vk::Result::eSuccess) {
		device.freeCommandBuffers(pool, commandBuffers);
		return commandBeginResult;
	}

	auto wrappedResult = commands(commandBuffer);
	if (wrappedResult != vk::Result::eSuccess) {
		device.freeCommandBuffers(pool, commandBuffers);
		return wrappedResult;
	}

	commandBuffer.end();

	vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &commandBuffer, 0, nullptr);

	graphicsQueue.submit(submitInfo);
	graphicsQueue.waitIdle();

	device.freeCommandBuffers(pool, commandBuffers);

	return vk::Result::eSuccess;
}

export struct SimpleGraphicsCommandPoolStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	requires   RowType::has_named_field<Row, BOOST_HANA_STRING("graphicsQueueFamilyIndex"), uint32_t>
			&& RowType::has_named_field<Row, BOOST_HANA_STRING("device"), vk::Device>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {

		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		uint32_t graphicsQueueFamilyIndex = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsQueueFamilyIndex"));

		vk::CommandPool pool = device.createCommandPool(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsQueueFamilyIndex));

		auto rWithPool = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("commandPool"), pool));
		auto result = f.applyRow(rWithPool);

		device.destroyCommandPool(pool);

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
		requires RowType::has_named_field<Row, BOOST_HANA_STRING("device"), vk::Device>
	          && RowType::has_named_field<Row, BOOST_HANA_STRING("commandPool"), vk::CommandPool>
		constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));

		std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, count_));

		auto rWithBuffers = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("commandBuffers"), commandBuffers));
		auto result = f.applyRow(rWithBuffers);

		// don't need to destroy command buffers
		return result;
	}
};

}