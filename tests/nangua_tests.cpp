

#include "bainangua.hpp"
#include "gtest/gtest.h"
#include "nangua_tests.hpp"

#include <expected.hpp>

import OneFrame;
import VulkanContext;
import PresentationLayer;
import Pipeline;
import Commands;
import VertexBuffer;

using namespace bainangua;

namespace {

TEST(VulkanContext, BasicTest)
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
					std::shared_ptr<PresentationLayer> presenter = buildPresentationLayer(s);

					s.endOfFrame();

					presenter->teardown();

					return true;
				}
			}
		)
	);

}

TEST(OneFrame, BasicTest)
{
	auto recordCommandBuffer = [](vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, std::shared_ptr<bainangua::PresentationLayer> presenter, const bainangua::PipelineBundle& pipeline)
		{
			vk::CommandBufferBeginInfo beginInfo({}, {});
			buffer.begin(beginInfo);

			std::array<vk::ClearValue, 1> clearColors{ vk::ClearValue() };

			vk::RenderPassBeginInfo renderPassInfo(
				pipeline.renderPass,
				swapChainImage,
				vk::Rect2D({ 0,0 }, presenter->swapChainExtent2D_),
				clearColors
			);
			buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

			buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipelines[0]);

			vk::Viewport viewport(
				0.0f,
				0.0f,
				static_cast<float>(presenter->swapChainExtent2D_.width),
				static_cast<float>(presenter->swapChainExtent2D_.height),
				0.0f,
				1.0f
			);
			buffer.setViewport(0, 1, &viewport);

			vk::Rect2D scissor({ 0,0 }, presenter->swapChainExtent2D_);
			buffer.setScissor(0, 1, &scissor);

			buffer.draw(3, 1, 0, 0);

			buffer.endRenderPass();

			buffer.end();
		};

	// renders 10 frames and then stops
	auto renderLoop = [&recordCommandBuffer](VulkanContext& s) -> bool {
			std::shared_ptr<PresentationLayer> presenter = buildPresentationLayer(s);

			std::filesystem::path shader_path = SHADER_DIR;
			tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createNoVertexPipeline(presenter, (shader_path / "Basic.vert_spv"), (shader_path / "Basic.frag_spv")));
			if (!pipelineResult.has_value()) {
				presenter->teardown();
				return false;
			}
			bainangua::PipelineBundle pipeline = pipelineResult.value();

			presenter->connectRenderPass(pipeline.renderPass);

			vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);

			bainangua::withCommandPool(s, poolInfo, [&](vk::CommandPool pool) {
				std::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, bainangua::MultiFrameCount));
				
				size_t framesLeft = 10;

				while (!glfwWindowShouldClose(s.glfwWindow) && framesLeft > 0) {

					size_t multiFrameIndex = framesLeft % bainangua::MultiFrameCount;

					auto result = bainangua::drawOneFrame(s, presenter, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
							recordCommandBuffer(commandbuffer, framebuffer, presenter, pipeline);
						});
					if (result.has_value()) {
						presenter = result.value();

						glfwPollEvents();

						s.endOfFrame();

						framesLeft--;
					}
					else {
						break;
					}
				}

				s.vkDevice.waitIdle();
			});

			bainangua::destroyPipeline(presenter->swapChainDevice_, pipeline);

			presenter->teardown();

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
		(tl::expected<int,std::string>(0))
	);

}


TEST(OneFrame, VertexBuffer)
{
	auto recordCommandBuffer = [](vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, std::shared_ptr<bainangua::PresentationLayer> presenter, const bainangua::PipelineBundle& pipeline, VkBuffer vertexBuffer)
		{
			vk::CommandBufferBeginInfo beginInfo({}, {});
			buffer.begin(beginInfo);

			std::array<vk::ClearValue, 1> clearColors{ vk::ClearValue() };

			vk::RenderPassBeginInfo renderPassInfo(
				pipeline.renderPass,
				swapChainImage,
				vk::Rect2D({ 0,0 }, presenter->swapChainExtent2D_),
				clearColors
			);
			buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

			buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipelines[0]);

			vk::Viewport viewport(
				0.0f,
				0.0f,
				static_cast<float>(presenter->swapChainExtent2D_.width),
				static_cast<float>(presenter->swapChainExtent2D_.height),
				0.0f,
				1.0f
			);
			buffer.setViewport(0, 1, &viewport);

			vk::Buffer vertexBuffers[] = { vertexBuffer };
			vk::DeviceSize offsets[] = { 0 };
			buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

			vk::Rect2D scissor({ 0,0 }, presenter->swapChainExtent2D_);
			buffer.setScissor(0, 1, &scissor);

			buffer.draw(3, 1, 0, 0);

			buffer.endRenderPass();

			buffer.end();
		};

	// renders 10 frames and then stops
	auto renderLoop = [&recordCommandBuffer](VulkanContext& s) -> bool {
		std::shared_ptr<PresentationLayer> presenter = buildPresentationLayer(s);

		std::filesystem::path shader_path = SHADER_DIR;
		tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createVTVertexPipeline(presenter, (shader_path / "PosColor.vert_spv"), (shader_path / "PosColor.frag_spv")));
		if (!pipelineResult.has_value()) {
			presenter->teardown();
			return false;
		}
		bainangua::PipelineBundle pipeline = pipelineResult.value();

		presenter->connectRenderPass(pipeline.renderPass);

		vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);

		bainangua::withCommandPool(s, poolInfo, [&](vk::CommandPool pool) {
			std::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, bainangua::MultiFrameCount));

			auto vertexResult = bainangua::createGPUVertexBuffer(s.vmaAllocator, s, pool, bainangua::staticVertices);
			auto [vertexBuffer, bufferMemory] = vertexResult.value();

			size_t framesLeft = 10;

			while (!glfwWindowShouldClose(s.glfwWindow) && framesLeft > 0) {

				size_t multiFrameIndex = framesLeft % bainangua::MultiFrameCount;

				auto result = bainangua::drawOneFrame(s, presenter, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
					recordCommandBuffer(commandbuffer, framebuffer, presenter, pipeline, vertexBuffer);
					});
				if (result.has_value()) {
					presenter = result.value();

					glfwPollEvents();

					s.endOfFrame();

					framesLeft--;
				}
				else {
					break;
				}
			}

			s.vkDevice.waitIdle();
			bainangua::destroyVertexBuffer(s.vmaAllocator, vertexBuffer, bufferMemory);
		});

		bainangua::destroyPipeline(presenter->swapChainDevice_, pipeline);

		presenter->teardown();

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
		(tl::expected<int, std::string>(0))
	);

}

TEST(OneFrame, IndexBuffer)
{
	auto recordCommandBuffer = [](vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, std::shared_ptr<bainangua::PresentationLayer> presenter, const bainangua::PipelineBundle& pipeline, VkBuffer vertexBuffer, VkBuffer indexBuffer) {
		vk::CommandBufferBeginInfo beginInfo({}, {});
		buffer.begin(beginInfo);

		std::array<vk::ClearValue, 1> clearColors{ vk::ClearValue() };

		vk::RenderPassBeginInfo renderPassInfo(
			pipeline.renderPass,
			swapChainImage,
			vk::Rect2D({ 0,0 }, presenter->swapChainExtent2D_),
			clearColors
		);
		buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

		buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipelines[0]);

		vk::Viewport viewport(
			0.0f,
			0.0f,
			static_cast<float>(presenter->swapChainExtent2D_.width),
			static_cast<float>(presenter->swapChainExtent2D_.height),
			0.0f,
			1.0f
		);
		buffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor({ 0,0 }, presenter->swapChainExtent2D_);
		buffer.setScissor(0, 1, &scissor);

		vk::Buffer vertexBuffers[] = { vertexBuffer };
		vk::DeviceSize offsets[] = { 0 };
		buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

		buffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

		buffer.drawIndexed(static_cast<uint32_t>(bainangua::staticIndices.size()), 1, 0, 0, 0);

		buffer.endRenderPass();

		buffer.end();
		};

	auto renderLoop = [=](bainangua::VulkanContext& s) -> bool {
		std::shared_ptr<bainangua::PresentationLayer> presenter = buildPresentationLayer(s);

		std::filesystem::path shader_path = SHADER_DIR; // defined via CMake in white_pumpkin.hpp
		tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createVTVertexPipeline(presenter, (shader_path / "PosColor.vert_spv"), (shader_path / "PosColor.frag_spv")));
		if (!pipelineResult.has_value()) {
			presenter->teardown();
			return false;
		}
		bainangua::PipelineBundle pipeline = pipelineResult.value();

		presenter->connectRenderPass(pipeline.renderPass);

		vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, s.graphicsQueueFamilyIndex);

		bainangua::withCommandPool(s, poolInfo, [&](vk::CommandPool pool) {
			auto vertexResult = bainangua::createGPUVertexBuffer(s.vmaAllocator, s, pool, bainangua::indexedStaticVertices);
			auto [vertexBuffer, bufferMemory] = vertexResult.value();

			auto indexResult = bainangua::createGPUIndexBuffer(s.vmaAllocator, s, pool, bainangua::staticIndices);
			auto [indexBuffer, indexBufferMemory] = indexResult.value();

			std::pmr::vector<vk::CommandBuffer> commandBuffers = s.vkDevice.allocateCommandBuffers<std::pmr::polymorphic_allocator<vk::CommandBuffer>>(vk::CommandBufferAllocateInfo(pool, vk::CommandBufferLevel::ePrimary, bainangua::MultiFrameCount));

			size_t multiFrameIndex = 0;
			unsigned int framesLeft = 10;

			while (!glfwWindowShouldClose(s.glfwWindow) && framesLeft > 0) {

				tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result> result = bainangua::drawOneFrame(s, presenter, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
						recordCommandBuffer(commandbuffer, framebuffer, presenter, pipeline, vertexBuffer, indexBuffer);
					});
				if (result.has_value()) {
					presenter = result.value();

					glfwPollEvents();

					s.endOfFrame();

					multiFrameIndex = (multiFrameIndex + 1) % bainangua::MultiFrameCount;

					framesLeft--;
				}
				else {
					break;
				}
			}

			s.vkDevice.waitIdle();

			bainangua::destroyVertexBuffer(s.vmaAllocator, vertexBuffer, bufferMemory);
			bainangua::destroyVertexBuffer(s.vmaAllocator, indexBuffer, indexBufferMemory);
		});

		bainangua::destroyPipeline(presenter->swapChainDevice_, pipeline);

		presenter->teardown();

		return true;
	};

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
		(tl::expected<int, std::string>(0))
	);

}

}