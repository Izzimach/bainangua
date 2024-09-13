module;

#include "bainangua.hpp"
#include "Commands.hpp"
#include "OuterBoilerplate.hpp"
#include "Pipeline.hpp"
#include "PresentationLayer.hpp"


#include "bainangua.hpp"

#include <functional>

export module OneFrame;

namespace bainangua {

export
vk::Result drawOneFrame(OuterBoilerplateState& s, PresentationLayer& presenter, const PipelineBundle& pipeline, vk::CommandBuffer buffer, size_t multiFrameIndex, std::function<void(vk::CommandBuffer, vk::Framebuffer)> drawCommands)
{
	vk::Result waitResult = s.vkDevice.waitForFences(presenter.inFlightFences_[multiFrameIndex], vk::True, UINT64_MAX);
	if (waitResult != vk::Result::eSuccess)
	{
		return waitResult;
	}

	// we don't use the "enhanced" version of acquireNextImageKHR since it throws on an OutOfDateKHR result
	uint32_t imageIndex;
	vk::Result acquireResult = s.vkDevice.acquireNextImageKHR(presenter.swapChain_.value(), UINT64_MAX, presenter.imageAvailableSemaphores_[multiFrameIndex], VK_NULL_HANDLE, &imageIndex);
	if (acquireResult == vk::Result::eErrorOutOfDateKHR || acquireResult == vk::Result::eSuboptimalKHR || s.windowResized) {
		s.windowResized = false;
		presenter.rebuildSwapChain(s);
		presenter.connectRenderPass(pipeline.renderPass);
		return acquireResult;
	}
 else if (acquireResult != vk::Result::eSuccess) {
  return acquireResult;
}

s.vkDevice.resetFences(presenter.inFlightFences_[multiFrameIndex]);

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
auto presentResult = s.presentQueue.presentKHR(&presentInfo);
if (presentResult != vk::Result::eSuccess)
{
	return presentResult;
}

return vk::Result::eSuccess;
}

}
