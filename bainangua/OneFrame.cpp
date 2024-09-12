
#include "bainangua.hpp"
#include "OneFrame.hpp"

#include <functional>

namespace bainangua {

vk::Result drawOneFrame(const OuterBoilerplateState &s, const PresentationLayer& presenter, vk::CommandBuffer buffer, std::function<void(vk::CommandBuffer, vk::Framebuffer)> drawCommands)
{
	auto [acquireResult, imageIndex] = s.vkDevice.acquireNextImageKHR(presenter.swapChain_.value(), UINT64_MAX, presenter.imageAvailableSemaphore_, VK_NULL_HANDLE);
	if (acquireResult != vk::Result::eSuccess)
	{
		return acquireResult;
	}

	buffer.reset();
	drawCommands(buffer, presenter.swapChainFramebuffers_[imageIndex]);

	std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	vk::SubmitInfo submitInfo(presenter.imageAvailableSemaphore_, waitStages, buffer, presenter.renderFinishedSemaphore_);
	s.graphicsQueue.submit(submitInfo, presenter.inFlightFence_);

	vk::PresentInfoKHR presentInfo(
		presenter.renderFinishedSemaphore_,
		presenter.swapChain_.value(),
		imageIndex,
		nullptr
	);
	auto presentResult = s.presentQueue.presentKHR(presentInfo);
	if (presentResult != vk::Result::eSuccess)
	{
		return presentResult;
	}

	vk::Result waitResult = s.vkDevice.waitForFences(presenter.inFlightFence_, vk::True, UINT64_MAX);
	if (waitResult != vk::Result::eSuccess)
	{
		return waitResult;
	}

	s.vkDevice.resetFences(presenter.inFlightFence_);

	return vk::Result::eSuccess;
}

}