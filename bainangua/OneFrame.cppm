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

}
