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


	vk::Format swapChainFormat_;
	vk::Extent2D swapChainExtent2D_;
	unsigned int swapChainImageCount_;

	std::optional<vk::Device> swapChainDevice_;
	std::optional<vk::SwapchainKHR> swapChain_;

private:
	void connectRenderPass(vk::RenderPass& renderPass);

	void teardownFramebuffers();

	std::vector<vk::Image> swapChainImages_;
	std::vector<vk::ImageView> swapChainImageViews_;
	std::vector<vk::Framebuffer> swapChainFramebuffers_;
};

}