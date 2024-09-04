//
// The presentation layer handles:
// - a (the?) swapchain
// - swapchain images
//

module;

#include "bainangua.hpp"

#include <vector>
#include <ranges>
#include <optional>

export module PresentationLayer;

import OuterBoilerplate;

struct SwapChainProperties
{
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};

SwapChainProperties querySwapChainProperties(const OuterBoilerplateState& boilerplate)
{
	vk::SurfaceCapabilitiesKHR capabilities = boilerplate.vkPhysicalDevice.getSurfaceCapabilitiesKHR(boilerplate.vkSurface);
	std::vector<vk::SurfaceFormatKHR> formats = boilerplate.vkPhysicalDevice.getSurfaceFormatsKHR(boilerplate.vkSurface);
	std::vector<vk::PresentModeKHR> presentModes = boilerplate.vkPhysicalDevice.getSurfacePresentModesKHR(boilerplate.vkSurface);

	return SwapChainProperties(capabilities, formats, presentModes);
}

vk::Extent2D chooseSwapChainImageExtent(const OuterBoilerplateState& boilerplate, const SwapChainProperties& swapChainProperties)
{
	if (swapChainProperties.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return swapChainProperties.capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(boilerplate.glfwWindow, &width, &height);

		const auto minExtent = swapChainProperties.capabilities.minImageExtent;
		const auto maxExtent = swapChainProperties.capabilities.maxImageExtent;
		return vk::Extent2D(
				std::clamp(static_cast<uint32_t>(width), minExtent.width, maxExtent.width),
				std::clamp(static_cast<uint32_t>(height), minExtent.height, maxExtent.height)
			);
	}
}

uint32_t chooseSwapChainImageCount(const OuterBoilerplateState& boilerplate, const SwapChainProperties& swapChainProperties)
{
	const uint32_t minImageCount = swapChainProperties.capabilities.minImageCount;
	const uint32_t maxImageCount = swapChainProperties.capabilities.maxImageCount;
	if (maxImageCount > 0)
	{
		return std::min(minImageCount + 1, maxImageCount);
	}
	return minImageCount + 1;
}

export
class PresentationLayer
{
	PresentationLayer() {}
	~PresentationLayer() { teardown(); }

	void build(OuterBoilerplateState& boilerplate)
	{
		using namespace std::views;
		SwapChainProperties swapChainInfo;
		auto useableFormats = swapChainInfo.formats | filter([](vk::SurfaceFormatKHR& s) {return s.format == vk::Format::eB8G8R8A8Srgb && s.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
		auto useableModes = swapChainInfo.presentModes | filter([](vk::PresentModeKHR& s) {return s == vk::PresentModeKHR::eFifo; });

		assert(!useableFormats.empty() && !useableModes.empty());
		swapChainFormat_ = useableFormats.front().format;

		swapChainImageCount_ = chooseSwapChainImageCount(boilerplate, swapChainInfo);
		swapChainExtent2D_ = chooseSwapChainImageExtent(boilerplate, swapChainInfo);
		std::vector<uint32_t> queueFamilies;

		vk::SwapchainCreateInfoKHR createInfo(
			vk::SwapchainCreateFlagsKHR(),
			boilerplate.vkSurface,
			swapChainImageCount_,
			swapChainFormat_,
			useableFormats.front().colorSpace,
			swapChainExtent2D_,
			1,
			vk::ImageUsageFlagBits::eColorAttachment,
			vk::SharingMode::eExclusive,
			queueFamilies,
			swapChainInfo.capabilities.currentTransform,
			vk::CompositeAlphaFlagBitsKHR::eOpaque,
			vk::PresentModeKHR::eFifo,
			true);
		swapChainDevice_ = boilerplate.vkDevice;
		swapChain_ = boilerplate.vkDevice.createSwapchainKHR(createInfo);

		// get swapchain images
		uint32_t imageCount;
		vkGetSwapchainImagesKHR(swapChainDevice_.value(), swapChain_.value(), &imageCount, nullptr);
		std::vector<VkImage> swapChainImagesRaw(imageCount);
		vkGetSwapchainImagesKHR(swapChainDevice_.value(), swapChain_.value(), &imageCount, swapChainImagesRaw.data());

		// convert the from VkImage to vk::Image
		swapChainImages_.resize(imageCount);
		std::ranges::transform(swapChainImagesRaw, swapChainImages_.begin(),
			[](VkImage v) { return vk::Image(v); });

		swapChainImageViews_.resize(imageCount);
		std::ranges::transform(swapChainImages_, swapChainImageViews_.begin(),
			[&](vk::Image i) {
				vk::ImageViewCreateInfo viewInfo({}, i, vk::ImageViewType::e2D, swapChainFormat_, vk::ComponentMapping(), vk::ImageSubresourceRange(vk::ImageAspectFlags(), 0, 1, 0, 1), nullptr);
				return swapChainDevice_.value().createImageView(viewInfo);
			});
	}

	void connectRenderPass(vk::RenderPass& renderPass)
	{
		teardownFramebuffers();
		swapChainFramebuffers_.resize(swapChainImageCount_);
		std::ranges::transform(swapChainImageViews_, swapChainFramebuffers_.begin(),
			[&](vk::ImageView iv) {
				vk::FramebufferCreateInfo framebufferInfo({}, renderPass, iv, swapChainExtent2D_.width, swapChainExtent2D_.height, 1);
				return swapChainDevice_.value().createFramebuffer(framebufferInfo);
			});
	}

	void teardownFramebuffers()
	{
		std::ranges::for_each(swapChainFramebuffers_,
			[&](vk::Framebuffer f) {
				swapChainDevice_.value().destroyFramebuffer(f);
			});
		swapChainFramebuffers_.clear();
	}

	void teardown()
	{
		if (swapChainDevice_ && swapChain_)
		{
			teardownFramebuffers();
			std::ranges::for_each(swapChainImageViews_,
				[&](vk::ImageView iv) {
					swapChainDevice_.value().destroyImageView(iv);
				});
			swapChainImageViews_.clear();
			swapChainDevice_.value().destroySwapchainKHR(swapChain_.value());
			swapChain_.reset();
		}
	}
	
private:
	std::optional<vk::Device> swapChainDevice_;
	std::optional<vk::SwapchainKHR> swapChain_;

	std::vector<vk::Image> swapChainImages_;
	std::vector<vk::ImageView> swapChainImageViews_;
	std::vector<vk::Framebuffer> swapChainFramebuffers_;
	vk::Format swapChainFormat_;
	vk::Extent2D swapChainExtent2D_;
	unsigned int swapChainImageCount_;
};

