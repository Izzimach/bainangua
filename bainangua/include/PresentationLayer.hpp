#pragma once

#include <vector>
#include <ranges>
#include <optional>

#include "OuterBoilerplate.hpp"

namespace bainangua {

struct PresentationLayer
{
	PresentationLayer() {}
	~PresentationLayer() { teardown(); }

	void build(OuterBoilerplateState& boilerplate);
	void teardown();

	void connectRenderPass(vk::RenderPass& renderPass);

	vk::Format swapChainFormat_;
	vk::Extent2D swapChainExtent2D_;
	unsigned int swapChainImageCount_;

	std::optional<vk::Device> swapChainDevice_;
	std::optional<vk::SwapchainKHR> swapChain_;

	std::vector<vk::Framebuffer> swapChainFramebuffers_;

	vk::Semaphore imageAvailableSemaphore_;
	vk::Semaphore renderFinishedSemaphore_;
	vk::Fence inFlightFence_;

private:

	void teardownFramebuffers();

	std::vector<vk::Image> swapChainImages_;
	std::vector<vk::ImageView> swapChainImageViews_;

};

}