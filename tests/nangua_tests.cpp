

#include "bainangua.hpp"
#include "gtest/gtest.h"
#include "nangua_tests.hpp"

import OneFrame;
import VulkanContext;
import PresentationLayer;
import Pipeline;
import Commands;

using namespace bainangua;

namespace {

TEST(Boilerplate, BasicTest)
{
	EXPECT_NO_THROW(
		createVulkanContext(
			VulkanContextConfig{
				.AppName = "Boilerplate Test App",
				.requiredExtensions = {
						VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
				},
				.useValidation = false,
				.innerCode = [](auto s) { return false; }
			}
		)
	);
}

TEST(PresentationLayer, BasicTest)
{
	EXPECT_NO_THROW(
		createVulkanContext(
			VulkanContextConfig{
				.AppName = "PresentationLayer Test App",
				.requiredExtensions = {
						VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
				},
				.useValidation = false,
				.innerCode = [](VulkanContext& s) -> bool {
					PresentationLayer presenter;
					presenter.build(s);

					s.endOfFrame();

					presenter.teardown();

					return true;
				}
			}
		)
	);

}

TEST(OneFrame, BasicTest)
{
	auto recordCommandBuffer = [](vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, const bainangua::PresentationLayer& presenter, const bainangua::PipelineBundle& pipeline)
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
		};

	// renders 10 frames and then stops
	auto renderLoop = [&recordCommandBuffer](VulkanContext& s) -> bool {
			bainangua::PresentationLayer presenter;
			presenter.build(s);

			std::filesystem::path shader_path = SHADER_DIR;
			bainangua::PipelineBundle pipeline(bainangua::createPipeline(presenter, (shader_path / "Basic.vert_spv"), (shader_path / "Basic.frag_spv")));

			presenter.connectRenderPass(pipeline.renderPass);

			vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);

			bainangua::withCommandPool(s, poolInfo, [&](vk::CommandPool pool) {
				std::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, bainangua::MultiFrameCount));
				
				size_t framesLeft = 10;

				while (!glfwWindowShouldClose(s.glfwWindow) && framesLeft > 0) {

					size_t multiFrameIndex = framesLeft % bainangua::MultiFrameCount;

					auto result = bainangua::drawOneFrame(s, presenter, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
							recordCommandBuffer(commandbuffer, framebuffer, presenter, pipeline);
						});
					if (result != vk::Result::eSuccess) {
						break;
					}

					glfwPollEvents();

					s.endOfFrame();
					framesLeft--;
				}
				s.vkDevice.waitIdle();

				});

			bainangua::destroyPipeline(presenter, pipeline);

			presenter.teardown();

			return true;
		};
	
	EXPECT_NO_THROW(
		createVulkanContext(
			VulkanContextConfig{
				.AppName = "OneFrame Test App",
				.requiredExtensions = {
						VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
				},
				.useValidation = false,
				.innerCode = renderLoop
			}
		)
	);
	EXPECT_EQ(
		createVulkanContext(
			VulkanContextConfig{
				.AppName = "OneFrame Test App",
				.requiredExtensions = {
						VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
						VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
				},
				.useValidation = false,
				.innerCode = renderLoop
			}
		),
		0
	);


}

}