
// 白南瓜 test application main entry point

#include "bainangua.hpp"
#include "expected.hpp" // using tl::expected since this is C++20
#include "RowType.hpp"
#include "tanuki.hpp"
#include "white_pumpkin.hpp"

#ifdef NDEBUG
#include <windows.h>
#endif

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <format>
#include <iostream>
#include <memory_resource>
#include <vector>

import Commands;
import VulkanContext;
import OneFrame;
import Pipeline;
import PresentationLayer;
import VertexBuffer;
import UniformBuffer;
import DescriptorSets;

void recordCommandBuffer(vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, std::shared_ptr<bainangua::PresentationLayer> presenterptr, const bainangua::PipelineBundle &pipeline, VkBuffer vertexBuffer, VkBuffer indexBuffer, vk::DescriptorSet uboDescriptorSet) {
	vk::CommandBufferBeginInfo beginInfo({}, {});
	buffer.begin(beginInfo);

	std::array<vk::ClearValue, 1> clearColors{ vk::ClearValue() };

	vk::RenderPassBeginInfo renderPassInfo(
		pipeline.renderPass,
		swapChainImage,
		vk::Rect2D({ 0,0 }, presenterptr->swapChainExtent2D_),
		clearColors
	);
	buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

	buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipelines[0]);

	vk::Viewport viewport(
		0.0f,
		0.0f,
		static_cast<float>(presenterptr->swapChainExtent2D_.width),
		static_cast<float>(presenterptr->swapChainExtent2D_.height),
		0.0f,
		1.0f
		);
	buffer.setViewport(0, 1, &viewport);

	vk::Rect2D scissor({ 0,0 }, presenterptr->swapChainExtent2D_);
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


template <typename Row>
auto renderLoop(Row r) -> tl::expected<int, std::pmr::string> {
	bainangua::VulkanContext &s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
	std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));
	bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));

	vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));
	std::pmr::vector<vk::CommandBuffer> commandBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("commandBuffers"));

	auto [vertexBuffer, bufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("vertexBuffer"));
	auto [indexBuffer, indexBufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexBuffer"));

	//vk::DescriptorPool descriptorPool = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorPool"));
	std::pmr::vector<vk::DescriptorSet> descriptorSets = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorSets"));
	std::pmr::vector<bainangua::UniformBufferBundle> uniformBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("uniformBuffers"));


	size_t multiFrameIndex = 0;

	while (!glfwWindowShouldClose(s.glfwWindow)) {

		tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result> result =
			bainangua::drawOneFrame(s, presenterptr, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
				updateUniformBuffer(*presenterptr, uniformBuffers[multiFrameIndex]);
				recordCommandBuffer(commandbuffer, framebuffer, presenterptr, pipeline, vertexBuffer, indexBuffer, descriptorSets[multiFrameIndex]);
			})
			.and_then([&](std::shared_ptr<bainangua::PresentationLayer> newPresenter) {
				presenterptr = newPresenter;

				glfwPollEvents();
				s.endOfFrame();
				multiFrameIndex = (multiFrameIndex + 1) % bainangua::MultiFrameCount;

				return tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result>(newPresenter);
			});
		if (!result) break;
	}

	s.vkDevice.waitIdle();

	return 0;
}

struct InvokeRenderLoop {
	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<int, std::pmr::string>;

	template<typename Row>
	constexpr tl::expected<int, std::pmr::string> applyRow(Row r) { return renderLoop(r); }
};



#ifdef NDEBUG
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#else
int main()
#endif
{
	// setup a pool arena for memory allocation.
	// Many of the vulkan-hpp calls use the default allocator so we have to call set_default_resource here.
	//
	std::pmr::synchronized_pool_resource default_pmr_resource;
	std::pmr::polymorphic_allocator<> default_pmr_allocator(&default_pmr_resource);
	std::pmr::set_default_resource(&default_pmr_resource);

	tl::expected<int, std::string> programResult = bainangua::createVulkanContext(
		bainangua::VulkanContextConfig{
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
			.innerCode = [=](bainangua::VulkanContext& s) -> bool {
				auto stageRow = boost::hana::make_map(
					boost::hana::make_pair(BOOST_HANA_STRING("context"), s)
				);
				auto stages =
					bainangua::PresentationLayerStage()
					| bainangua::MVPPipelineStage(SHADER_DIR)
					| bainangua::SimpleGraphicsCommandPoolStage()
					| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
					| bainangua::GPUVertexBufferStage()
					| bainangua::GPUIndexBufferStage()
					| bainangua::CreateSimpleDescriptorPoolStage(vk::DescriptorType::eUniformBuffer, bainangua::MultiFrameCount)
					| bainangua::CreateSimpleDescriptorSetsStage(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, bainangua::MultiFrameCount)
					| bainangua::CreateAndLinkUniformBuffersStage()
					| InvokeRenderLoop();
				
				auto stageResult = stages.applyRow(stageRow);

				return true;
			}
		}
	);

	if (programResult.has_value()) {
		return programResult.value();
	}
	else {
		std::cout << std::format("program error: {}\n", programResult.error());
		return -1;
	}
}
