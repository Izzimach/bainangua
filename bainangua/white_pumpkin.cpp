
// 白南瓜 test application main entry point

#include "bainangua.hpp"
#include "expected.hpp" // using tl::expected since this is C++20
#include "RowType.hpp"
#include "tanuki.hpp"
#include "white_pumpkin.hpp"


#include <algorithm>
#include <concepts>
#include <filesystem>
#include <format>
#include <iostream>
#include <boost/hana/map.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/define_struct.hpp>
#include <coro/coro.hpp>
#include <map>
#include <utility>
#include <vector>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <reflect.hpp>


#ifdef NDEBUG
#include <windows.h>
#endif

import Commands;
import VulkanContext;
import OneFrame;
import Pipeline;
import PresentationLayer;
import VertBuffer;
import UniformBuffer;
import DescriptorSets;
import TextureImage;
import ResourceLoader;
import Shader;
import StagingBuffer;
import VertexBuffer;
import CommandQueue;

void recordCommandBuffer(vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, vk::Extent2D swapChainExtent, const bainangua::PipelineBundle &pipeline, VkBuffer vertexBuffer, VkBuffer indexBuffer, vk::DescriptorSet uboDescriptorSet) {
	vk::CommandBufferBeginInfo beginInfo({}, {});
	buffer.begin(beginInfo);

	std::array<vk::ClearValue, 1> clearColors{ vk::ClearValue() };

	vk::RenderPassBeginInfo renderPassInfo(
		pipeline.renderPass,
		swapChainImage,
		vk::Rect2D({ 0,0 }, swapChainExtent),
		clearColors
	);
	buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

	buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipelines[0]);

	vk::Viewport viewport(
		0.0f,
		0.0f,
		static_cast<float>(swapChainExtent.width),
		static_cast<float>(swapChainExtent.height),
		0.0f,
		1.0f
		);
	buffer.setViewport(0, 1, &viewport);

	vk::Rect2D scissor({ 0,0 }, swapChainExtent);
	buffer.setScissor(0, 1, &scissor);

	vk::Buffer vertexBuffers[] = { vertexBuffer };
	vk::DeviceSize offsets[] = { 0 };
	buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

	buffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

	buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, 1, &uboDescriptorSet,0,nullptr);

	buffer.drawIndexed(static_cast<uint32_t>(bainangua::staticIndices.size()), 1, 0, 0, 0);

	buffer.endRenderPass();

	buffer.end();
}

struct NoRenderLoop {
	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<int, std::string>;

	template<typename Row>
	constexpr tl::expected<int, std::string> applyRow(Row r) {
		std::coroutine_handle<> endOfFrame = boost::hana::at_key(r, BOOST_HANA_STRING("endOfFrameCallback"));
		endOfFrame();
		return 0;
	}
};


struct DrawMVPIndexedGeometry {
	using row_tag = RowType::RowFunctionTag;
	using return_type = void;

	template<typename Row>
	constexpr void applyRow(Row r) {
		vk::CommandBuffer buffer = boost::hana::at_key(r, BOOST_HANA_STRING("primaryCommandBuffer"));
		bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));
		auto [vertexBuffer, bufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexedVertexBuffer"));
		auto [indexBuffer, indexBufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexBuffer"));
		std::vector<vk::DescriptorSet> descriptorSets = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorSets"));
		size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));

		vk::Buffer vertexBuffers[] = { vertexBuffer };
		vk::DeviceSize offsets[] = { 0 };
		buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

		buffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

		buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, 1, &(descriptorSets[multiFrameIndex]), 0, nullptr);

		buffer.drawIndexed(static_cast<uint32_t>(bainangua::staticIndices.size()), 1, 0, 0, 0);
	}
};

struct UpdateUniformBuffer {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		std::vector<bainangua::UniformBufferBundle> uniformBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("uniformBuffers"));
		size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));
		vk::Extent2D viewportExtent = boost::hana::at_key(r, BOOST_HANA_STRING("viewportExtent"));

		bainangua::updateUniformBuffer(viewportExtent, uniformBuffers[multiFrameIndex]);

		return f.applyRow(r);
	}
};






template <typename Row>
auto renderLoop (Row r) -> bainangua::bng_expected<bool> {
	vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
	vk::Queue graphicsQueue = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsQueue"));
	vk::Queue presentQueue = boost::hana::at_key(r, BOOST_HANA_STRING("presentQueue"));
	GLFWwindow* glfwWindow = boost::hana::at_key(r, BOOST_HANA_STRING("glfwWindow"));
	std::coroutine_handle<> endOfFrame = boost::hana::at_key(r, BOOST_HANA_STRING("endOfFrameCallback"));
	std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));
	bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));

	vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));
	std::vector<vk::CommandBuffer> commandBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("commandBuffers"));

	auto [vertexBuffer, bufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("vertexBuffer"));
	auto [indexBuffer, indexBufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexBuffer"));

	//vk::DescriptorPool descriptorPool = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorPool"));
	std::vector<vk::DescriptorSet> descriptorSets = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorSets"));
	std::vector<bainangua::UniformBufferBundle> uniformBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("uniformBuffers"));


	size_t multiFrameIndex = 0;

	while (!glfwWindowShouldClose(glfwWindow)) {

		tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result> result =
			bainangua::drawOneFrame(device, graphicsQueue, presentQueue, presenterptr, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
				updateUniformBuffer(presenterptr->swapChainExtent2D_, uniformBuffers[multiFrameIndex]);
				recordCommandBuffer(commandbuffer, framebuffer, presenterptr->swapChainExtent2D_, pipeline, vertexBuffer, indexBuffer, descriptorSets[multiFrameIndex]);
				})
			.and_then([&](std::shared_ptr<bainangua::PresentationLayer> newPresenter) {
				presenterptr = newPresenter;

				glfwPollEvents();
				endOfFrame();
				multiFrameIndex = (multiFrameIndex + 1) % bainangua::MultiFrameCount;

				return tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result>(newPresenter);
			});
		if (!result) {
			device.waitIdle();
			return formatVkResultError("error presenting frame", result.error());
		}
	}

	device.waitIdle();

	return 0;
};

struct InvokeRenderLoop {
	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<bool>;

	template<typename Row>
	constexpr bainangua::bng_expected<bool> applyRow(Row r) { return renderLoop(r); }
};

struct InnerFunction {
	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<bool>;

	template <typename Row>
	requires   RowType::has_named_field<Row, BOOST_HANA_STRING("device"), vk::Device>
			&& RowType::has_named_field<Row, BOOST_HANA_STRING("commandBuffers"), std::vector<vk::CommandBuffer>>
			&& RowType::has_named_field<Row, BOOST_HANA_STRING("graphicsFunnel"), std::shared_ptr<bainangua::CommandQueueFunnel>>
	constexpr bainangua::bng_expected<bool> applyRow(Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		std::vector<vk::CommandBuffer> commandBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("commandBuffers"));
		std::shared_ptr<bainangua::CommandQueueFunnel> graphicsQueue = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsFunnel"));

		vk::CommandBuffer cmd = commandBuffers[0];

		vk::CommandBufferBeginInfo beginInfo({}, {});
		cmd.begin(beginInfo);
		cmd.end();

		vk::SubmitInfo submit(0, nullptr, {}, 1, &cmd, 0, nullptr, nullptr);

		coro::event e;

		auto completionTask = [](coro::event& e) -> coro::task<void> {
			e.set();
			co_return;
		};

		auto awaiterTask = [](const coro::event& e) -> coro::task<void> {
			co_await e;
			co_return;
		};

		graphicsQueue->awaitCommand(submit, completionTask(e));

		coro::sync_wait(awaiterTask(e));

		device.waitIdle();
		
		return true;
	}
};


#ifdef NDEBUG
int WINAPI wWinMain(HINSTANCE , HINSTANCE , PWSTR , int )
#else
int main()
#endif
{
	auto config = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("config"), bainangua::VulkanContextConfig{
				.AppName = "My Test App",
				.requiredExtensions = {
					VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
					VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
				},
	#if NDEBUG
				.useValidation = false,
	#else
				.useValidation = true,
	#endif
			}
		)
	);

	auto loaderDirectory = boost::hana::make_map(
		bainangua::shaderLoader
	);

	auto loaderStorage = createLoaderStorage(loaderDirectory);

	auto program =
		bainangua::QuickCreateContext()
		| bainangua::ResourceLoaderStage(loaderDirectory, loaderStorage)
		| bainangua::CreateQueueFunnels()
		| bainangua::SimpleGraphicsCommandPoolStage()
		| bainangua::PrimaryGraphicsCommandBuffersStage(1)
		| RowType::RowWrapLambda<bainangua::bng_expected<bool>>([](auto row) { return true; });

	bainangua::bng_expected<bool> programResult = program.applyRow(config);

	if (programResult.has_value()) {
		return 0;
	}
	else {
		std::cout << std::format("program error: {}\n", programResult.error());
		return -1;
	}
}
