﻿
// 白南瓜 test application main entry point

#include "bainangua.hpp"
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

void recordCommandBuffer(vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, const bainangua::PresentationLayer &presenter, const bainangua::PipelineBundle &pipeline) {
	vk::CommandBufferBeginInfo beginInfo({}, {});
	buffer.begin(beginInfo);

	std::array<vk::ClearValue, 1> clearColors{ vk::ClearValue() };

	vk::RenderPassBeginInfo renderPassInfo(
		pipeline.renderPass,
		swapChainImage,
		vk::Rect2D({ 0,0 }, presenter.swapChainExtent2D_),
		clearColors
	);
	buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

	buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipelines[0]);

	vk::Viewport viewport(
		0.0f,
		0.0f,
		static_cast<float>(presenter.swapChainExtent2D_.width),
		static_cast<float>(presenter.swapChainExtent2D_.height),
		0.0f,
		1.0f
			);
			buffer.setViewport(0, 1, &viewport);

			vk::Rect2D scissor({ 0,0 }, presenter.swapChainExtent2D_);
			buffer.setScissor(0, 1, &scissor);

			buffer.draw(3, 1, 0, 0);

			buffer.endRenderPass();

			buffer.end();
}
int main()
{
	// setup a pool arena for memory allocation.
	// Many of the vulkan-hpp calls use the default allocator so we have to call set_default_resource here.
	//
	std::pmr::synchronized_pool_resource default_pmr_resource;
	std::pmr::polymorphic_allocator<> default_pmr_allocator(&default_pmr_resource);
	std::pmr::set_default_resource(&default_pmr_resource);

	RowType::testRowTypes();

	return bainangua::createVulkanContext(
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
				bainangua::PresentationLayer presenter = buildPresentationLayer(s).value();

				std::filesystem::path shader_path = SHADER_DIR; // defined via CMake in white_pumpkin.hpp
				bainangua::PipelineBundle pipeline(bainangua::createPipeline(presenter, (shader_path / "Basic.vert_spv"), (shader_path / "Basic.frag_spv")));

				presenter.connectRenderPass(pipeline.renderPass);

				vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);

				bainangua::withCommandPool(s, poolInfo, [&](vk::CommandPool pool) {
					std::pmr::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers<std::pmr::polymorphic_allocator<vk::CommandBuffer>>(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, bainangua::MultiFrameCount));

					size_t multiFrameIndex = 0;
					while (!glfwWindowShouldClose(s.glfwWindow)) {

						auto result = bainangua::drawOneFrame(s, presenter, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
								recordCommandBuffer(commandbuffer, framebuffer, presenter, pipeline);
							});
						if (result != vk::Result::eSuccess &&
							result != vk::Result::eErrorOutOfDateKHR) {
							break;
						}

						glfwPollEvents();

						s.endOfFrame();

						multiFrameIndex = (multiFrameIndex + 1) % bainangua::MultiFrameCount;
					}

					s.vkDevice.waitIdle();
				});

				bainangua::destroyPipeline(presenter, pipeline);

				presenter.teardown();

				return true;
			}
		}
	);
}
