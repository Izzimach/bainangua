
#include "bainangua.hpp"
#include "OneFrame.hpp"

#include <functional>

namespace bainangua {

vk::Result drawOneFrame(const OuterBoilerplateState &s, const PresentationLayer& presenter, vk::CommandBuffer buffer, size_t multiFrameIndex, std::function<void(vk::CommandBuffer, vk::Framebuffer)> drawCommands)
{
	vk::Result waitResult = s.vkDevice.waitForFences(presenter.inFlightFences_[multiFrameIndex], vk::True, UINT64_MAX);
	if (waitResult != vk::Result::eSuccess)
	{
		return waitResult;
	}

	s.vkDevice.resetFences(presenter.inFlightFences_[multiFrameIndex]);

	auto [acquireResult, imageIndex] = s.vkDevice.acquireNextImageKHR(presenter.swapChain_.value(), UINT64_MAX, presenter.imageAvailableSemaphores_[multiFrameIndex], VK_NULL_HANDLE);
	if (acquireResult != vk::Result::eSuccess)
	{
		return acquireResult;
	}

	buffer.reset();
	drawCommands(buffer, presenter.swapChainFramebuffers_[imageIndex]);

	std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	vk::SubmitInfo submitInfo(presenter.imageAvailableSemaphores_[multiFrameIndex], waitStages, buffer, presenter.renderFinishedSemaphores_[multiFrameIndex]);
	s.graphicsQueue.submit(submitInfo, presenter.inFlightFences_[multiFrameIndex]);

	vk::PresentInfoKHR presentInfo(
		presenter.renderFinishedSemaphores_[multiFrameIndex],
		presenter.swapChain_.value(),
		imageIndex,
		nullptr
	);
	auto presentResult = s.presentQueue.presentKHR(presentInfo);
	if (presentResult != vk::Result::eSuccess)
	{
		return presentResult;
	}

	return vk::Result::eSuccess;
}

}