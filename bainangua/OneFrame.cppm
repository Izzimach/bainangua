module;

#include "bainangua.hpp"
#include "RowType.hpp"

#include <expected.hpp>
#include <functional>

export module OneFrame;

import VulkanContext;
import PresentationLayer;
import Pipeline;

namespace bainangua {

export
tl::expected<std::shared_ptr<PresentationLayer>,vk::Result> drawOneFrame(VulkanContext& s, std::shared_ptr<PresentationLayer> presenterptr, const PipelineBundle& pipeline, vk::CommandBuffer buffer, size_t multiFrameIndex, std::function<void(vk::CommandBuffer, vk::Framebuffer)> drawCommands)
{
	uint32_t retryLimit = 100;

	// This is called from two different places.
	auto rebuildPresenter = [&]() {
			presenterptr = presenterptr->rebuildSwapChain(s);
			presenterptr->connectRenderPass(pipeline.renderPass);
		};

	while (retryLimit > 0) {
		retryLimit--;

		vk::Result waitResult = s.vkDevice.waitForFences(presenterptr->inFlightFences_[multiFrameIndex], vk::True, UINT64_MAX);
		if (waitResult != vk::Result::eSuccess)
		{
			return tl::make_unexpected(waitResult);
		}

		// we don't use the "enhanced" version of acquireNextImageKHR since it throws on an OutOfDateKHR result
		uint32_t imageIndex = 0;
		vk::Result acquireResult = s.vkDevice.acquireNextImageKHR(presenterptr->swapChain_, UINT64_MAX, presenterptr->imageAvailableSemaphores_[multiFrameIndex], VK_NULL_HANDLE, &imageIndex);
		if (acquireResult == vk::Result::eErrorOutOfDateKHR || acquireResult == vk::Result::eSuboptimalKHR || s.windowResized) {
			s.windowResized = false;
			rebuildPresenter();
			continue; // retry and rebuild swapchain
		}
		else if (acquireResult != vk::Result::eSuccess) {
			return tl::make_unexpected(acquireResult);
		}

		s.vkDevice.resetFences(presenterptr->inFlightFences_[multiFrameIndex]);

		buffer.reset();
		drawCommands(buffer, presenterptr->swapChainFramebuffers_[imageIndex]);

		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		vk::SubmitInfo submitInfo(presenterptr->imageAvailableSemaphores_[multiFrameIndex], waitStages, buffer, presenterptr->renderFinishedSemaphores_[multiFrameIndex]);
		s.graphicsQueue.submit(submitInfo, presenterptr->inFlightFences_[multiFrameIndex]);

		vk::PresentInfoKHR presentInfo(
			presenterptr->renderFinishedSemaphores_[multiFrameIndex],
			presenterptr->swapChain_,
			imageIndex,
			nullptr
		);
		vk::Result presentResult = s.presentQueue.presentKHR(&presentInfo);
		if (presentResult == vk::Result::eErrorOutOfDateKHR) {
			rebuildPresenter();
			continue; // retry the wait/acquire
		}
		else if (presentResult != vk::Result::eSuccess) {
			return tl::make_unexpected(presentResult);
		}
		break; // don't retry
	}

	// I hope we never hit the retry limit
	if (retryLimit == 0) {
		return tl::make_unexpected(vk::Result::eErrorUnknown);
	}

	return presenterptr;
}

export
struct StandardMultiFrameLoop {
	StandardMultiFrameLoop() = default;
	StandardMultiFrameLoop(size_t autoClose) : autoClose_(autoClose) {}

	std::optional<size_t> autoClose_; //< If present, then automatically close after this many frames.

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = bng_expected<bool>;

	template <typename RowFunction, typename Row>
	constexpr bng_expected<bool> wrapRowFunction(RowFunction f, Row r) {
		bainangua::VulkanContext& s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));
		bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));

		std::pmr::vector<vk::CommandBuffer> commandBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("commandBuffers"));


		size_t multiFrameIndex = 0;

		while (!glfwWindowShouldClose(s.glfwWindow)) {

			tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result> result =
				bainangua::drawOneFrame(s, presenterptr, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandBuffer, vk::Framebuffer frameBuffer) {
					auto newFields = boost::hana::make_map(
						boost::hana::make_pair(BOOST_HANA_STRING("primaryCommandBuffer"), commandBuffer),
						boost::hana::make_pair(BOOST_HANA_STRING("targetFrameBuffer"), frameBuffer),
						boost::hana::make_pair(BOOST_HANA_STRING("viewportExtent"), presenterptr->swapChainExtent2D_),
						boost::hana::make_pair(BOOST_HANA_STRING("multiFrameIndex"), multiFrameIndex)
					);
					auto rWithNewFields = boost::hana::fold_left(r, newFields, boost::hana::insert);

					auto drawResult = f.applyRow(rWithNewFields);
					/*updateUniformBuffer(presenterptr->swapChainExtent2D_, uniformBuffers[multiFrameIndex]);
					recordCommandBuffer(commandBuffer, frameBuffer, presenterptr->swapChainExtent2D_, pipeline, vertexBuffer, indexBuffer, descriptorSets[multiFrameIndex]);*/
				})
				.and_then([&](std::shared_ptr<bainangua::PresentationLayer> newPresenter) {
					presenterptr = newPresenter;

					glfwPollEvents();
					s.endOfFrame();
					multiFrameIndex = (multiFrameIndex + 1) % bainangua::MultiFrameCount;

					return tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result>(newPresenter);
				});
			if (!result) break;
			if (autoClose_.has_value() && --autoClose_.value() == 0) {
				break;
			}
		}

		s.vkDevice.waitIdle();

		return true;
	}
};

export
struct BasicRendering {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = tl::expected<int, std::pmr::string>;

	template <typename RowFunction, typename Row>
	constexpr tl::expected<int, std::pmr::string> wrapRowFunction(RowFunction f, Row r) {
		vk::CommandBuffer buffer = boost::hana::at_key(r, BOOST_HANA_STRING("primaryCommandBuffer"));
		vk::Framebuffer targetFrameBuffer = boost::hana::at_key(r, BOOST_HANA_STRING("targetFrameBuffer"));
		bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));
		vk::Extent2D viewportExtent = boost::hana::at_key(r, BOOST_HANA_STRING("viewportExtent"));

		vk::CommandBufferBeginInfo beginInfo({}, {});
		buffer.begin(beginInfo);

		std::array<vk::ClearValue, 1> clearColors{ vk::ClearValue() };

		vk::RenderPassBeginInfo renderPassInfo(
			pipeline.renderPass,
			targetFrameBuffer,
			vk::Rect2D({ 0,0 }, viewportExtent),
			clearColors
		);
		buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

		buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipelines[0]);

		vk::Viewport viewport(
			0.0f,
			0.0f,
			static_cast<float>(viewportExtent.width),
			static_cast<float>(viewportExtent.height),
			0.0f,
			1.0f
		);
		buffer.setViewport(0, 1, &viewport);

		vk::Rect2D scissor({ 0,0 }, viewportExtent);
		buffer.setScissor(0, 1, &scissor);

		f.applyRow(r);

		buffer.endRenderPass();

		buffer.end();

		return 0;
	}
};



}
