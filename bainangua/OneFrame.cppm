module;

#include "bainangua.hpp"

#include <expected.hpp>
#include <functional>

export module OneFrame;

import VulkanContext;
import PresentationLayer;
import Pipeline;

namespace bainangua {

export
tl::expected<PresentationLayer,vk::Result> drawOneFrame(VulkanContext& s, PresentationLayer presenter, const PipelineBundle& pipeline, vk::CommandBuffer buffer, size_t multiFrameIndex, std::function<void(vk::CommandBuffer, vk::Framebuffer)> drawCommands)
{
	uint32_t retryLimit = 100;

	// This is called from two different places.
	auto rebuildPresenter = [&]() {
			presenter = presenter.rebuildSwapChain(s).value();
			presenter.connectRenderPass(pipeline.renderPass);
		};

	while (retryLimit > 0) {
		retryLimit--;

		vk::Result waitResult = s.vkDevice.waitForFences(presenter.inFlightFences_[multiFrameIndex], vk::True, UINT64_MAX);
		if (waitResult != vk::Result::eSuccess)
		{
			presenter.teardown();
			return tl::make_unexpected(waitResult);
		}

		// we don't use the "enhanced" version of acquireNextImageKHR since it throws on an OutOfDateKHR result
		uint32_t imageIndex = 0;
		vk::Result acquireResult = s.vkDevice.acquireNextImageKHR(presenter.swapChain_, UINT64_MAX, presenter.imageAvailableSemaphores_[multiFrameIndex], VK_NULL_HANDLE, &imageIndex);
		if (acquireResult == vk::Result::eErrorOutOfDateKHR || acquireResult == vk::Result::eSuboptimalKHR || s.windowResized) {
			s.windowResized = false;
			rebuildPresenter();
			continue; // retry and rebuild swapchain
		}
		else if (acquireResult != vk::Result::eSuccess) {
			presenter.teardown();
			return tl::make_unexpected(acquireResult);
		}

		s.vkDevice.resetFences(presenter.inFlightFences_[multiFrameIndex]);

		buffer.reset();
		drawCommands(buffer, presenter.swapChainFramebuffers_[imageIndex]);

		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		vk::SubmitInfo submitInfo(presenter.imageAvailableSemaphores_[multiFrameIndex], waitStages, buffer, presenter.renderFinishedSemaphores_[multiFrameIndex]);
		s.graphicsQueue.submit(submitInfo, presenter.inFlightFences_[multiFrameIndex]);

		vk::PresentInfoKHR presentInfo(
			presenter.renderFinishedSemaphores_[multiFrameIndex],
			presenter.swapChain_,
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

	return presenter;
}

}
