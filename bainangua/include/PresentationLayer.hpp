#pragma once

#include <vector>
#include <ranges>
#include <optional>

#include "OuterBoilerplate.hpp"

namespace bainangua {

constexpr uint32_t MultiFrameCount = 2;

struct PresentationLayer
{
	PresentationLayer() {}
	~PresentationLayer() { teardown(); }

	void build(OuterBoilerplateState& boilerplate);
	void teardown();

	void connectRenderPass(const vk::RenderPass& renderPass);

	void rebuildSwapChain(OuterBoilerplateState &s);

	vk::Format swapChainFormat_;
	vk::Extent2D swapChainExtent2D_;
	unsigned int swapChainImageCount_;

	std::optional<vk::Device> swapChainDevice_;
	std::optional<vk::SwapchainKHR> swapChain_;

	std::vector<vk::Framebuffer> swapChainFramebuffers_;

	std::array<vk::Semaphore, MultiFrameCount> imageAvailableSemaphores_;
	std::array<vk::Semaphore, MultiFrameCount> renderFinishedSemaphores_;
	std::array<vk::Fence, MultiFrameCount> inFlightFences_;

private:

	void teardownFramebuffers();

	std::vector<vk::Image> swapChainImages_;
	std::vector<vk::ImageView> swapChainImageViews_;
};

}