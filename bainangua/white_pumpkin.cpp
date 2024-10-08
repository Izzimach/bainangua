﻿
// 白南瓜 test application main entry point

#include "bainangua.hpp"
#include "expected.hpp" // note: tl::expected for c++<23
#include "RowType.hpp"
#include "tanuki.hpp"
#include "white_pumpkin.hpp"

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <fmt/format.h>
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
auto renderLoop(Row r) -> tl::expected<int, std::string> {
	bainangua::VulkanContext &s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
	std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));

	std::filesystem::path shader_path = SHADER_DIR; // defined via CMake in white_pumpkin.hpp
	tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createMVPVertexPipeline(presenterptr, (shader_path / "PosColorMVP.vert_spv"), (shader_path / "PosColor.frag_spv")));
	if (!pipelineResult.has_value()) {
		return false;
	}
	bainangua::PipelineBundle pipeline = pipelineResult.value();

	presenterptr->connectRenderPass(pipeline.renderPass);

	vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);

	bainangua::withCommandPool(s, poolInfo, [&](vk::CommandPool pool) {
		auto vertexResult = bainangua::createGPUVertexBuffer(s.vmaAllocator, s, pool, bainangua::indexedStaticVertices);
		auto [vertexBuffer, bufferMemory] = vertexResult.value();

		auto indexResult = bainangua::createGPUIndexBuffer(s.vmaAllocator, s, pool, bainangua::staticIndices);
		auto [indexBuffer, indexBufferMemory] = indexResult.value();

		auto uniformBuffers = bainangua::createUniformBuffers(s.vmaAllocator).value();
		auto descriptorPoolResult = bainangua::createDescriptorPool(s);
		if (!descriptorPoolResult.has_value()) {
			return;
		}
		vk::DescriptorPool descriptorPool = descriptorPoolResult.value();
		vk::DescriptorSetLayout layout = bainangua::createDescriptorSetLayout(s.vkDevice).value();
		auto descriptorSets = bainangua::createDescriptorSets(s, descriptorPool, layout).value();
		auto linkResult = bainangua::linkUBOAndDescriptors(s, uniformBuffers, descriptorSets);

		std::pmr::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers<std::pmr::polymorphic_allocator<vk::CommandBuffer>>(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, bainangua::MultiFrameCount));

		size_t multiFrameIndex = 0;

		while (!glfwWindowShouldClose(s.glfwWindow)) {

			updateUniformBuffer(*presenterptr, uniformBuffers[multiFrameIndex]);
			tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result> result = bainangua::drawOneFrame(s, presenterptr, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
				recordCommandBuffer(commandbuffer, framebuffer, presenterptr, pipeline, vertexBuffer, indexBuffer, descriptorSets[multiFrameIndex]);
				});
			if (result.has_value()) {
				presenterptr = result.value();


				glfwPollEvents();

				s.endOfFrame();

				multiFrameIndex = (multiFrameIndex + 1) % bainangua::MultiFrameCount;

			}
			else {
				break;
			}
		}

		s.vkDevice.waitIdle();

		bainangua::destroyVertexBuffer(s.vmaAllocator, vertexBuffer, bufferMemory);
		bainangua::destroyVertexBuffer(s.vmaAllocator, indexBuffer, indexBufferMemory);
		for (auto& ubo : uniformBuffers) {
			bainangua::destroyUniformBuffer(s.vmaAllocator, ubo);
		}
		s.vkDevice.destroyDescriptorPool(descriptorPool);
		s.vkDevice.destroyDescriptorSetLayout(layout);
		});


	bainangua::destroyPipeline(s.vkDevice, pipeline);
}

struct InvokeRenderLoop {
	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<int, std::string>;

	template<typename Row>
	constexpr tl::expected<int, std::string> applyRow(Row r) { return renderLoop(r); }
};




int main()
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
		fmt::print("program error: {}\n", programResult.error());
		return -1;
	}
}
