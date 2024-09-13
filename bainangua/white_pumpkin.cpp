
// 白南瓜 test application main entry point

#include "bainangua.hpp"
#include "tanuki.hpp"
#include "white_pumpkin.hpp"

#include <algorithm>
#include <filesystem>
#include <fmt/format.h>
#include <vector>

import Commands;
import OuterBoilerplate;
import OneFrame;
import Pipeline;
import PresentationLayer;

void recordCommandBuffer(vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, const bainangua::PresentationLayer &presenter, const bainangua::PipelineBundle &pipeline)
{
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
	bainangua::outerBoilerplate(
		bainangua::OuterBoilerplateConfig{
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
			.innerCode = [](bainangua::OuterBoilerplateState& s) -> bool {
				bainangua::PresentationLayer presenter;
				presenter.build(s);

				std::filesystem::path shader_path = SHADER_DIR;
				bainangua::PipelineBundle pipeline(bainangua::createPipeline(presenter, (shader_path / "Basic.vert_spv"), (shader_path / "Basic.frag_spv")));

				presenter.connectRenderPass(pipeline.renderPass);

				vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);

				bainangua::withCommandPool(s, poolInfo, [&presenter, &s, &pipeline](vk::CommandPool pool) {
					std::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, bainangua::MultiFrameCount));

					size_t multiFrameIndex = 0;
					while (!glfwWindowShouldClose(s.glfwWindow)) {

						auto result = bainangua::drawOneFrame(s, presenter, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&presenter, &pipeline](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
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


	return 0;
}
