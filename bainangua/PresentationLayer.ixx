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
	PresentationLayer() : swapchain_() {}
	~PresentationLayer() { teardown(); }

	void build(OuterBoilerplateState& boilerplate)
	{
		using namespace std::views;
		SwapChainProperties swapChainInfo;
		auto useableFormats = swapChainInfo.formats | filter([](vk::SurfaceFormatKHR& s) {return s.format == vk::Format::eB8G8R8A8Srgb && s.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
		auto useableModes = swapChainInfo.presentModes | filter([](vk::PresentModeKHR& s) {return s == vk::PresentModeKHR::eFifo; });

		assert(!useableFormats.empty() && !useableModes.empty());

		uint32_t imageCount = chooseSwapChainImageCount(boilerplate, swapChainInfo);
		vk::Extent2D imageExtent = chooseSwapChainImageExtent(boilerplate, swapChainInfo);
		std::vector<uint32_t> queueFamilies;

		vk::SwapchainCreateInfoKHR createInfo(
			vk::SwapchainCreateFlagsKHR(),
			boilerplate.vkSurface,
			imageCount,
			useableFormats.front().format,
			useableFormats.front().colorSpace,
			imageExtent,
			1,
			vk::ImageUsageFlagBits::eColorAttachment,
			vk::SharingMode::eExclusive,
			queueFamilies,
			swapChainInfo.capabilities.currentTransform,
			vk::CompositeAlphaFlagBitsKHR::eOpaque,
			vk::PresentModeKHR::eFifo,
			true);
		swapchainDevice_ = boilerplate.vkDevice;
		swapchain_ = boilerplate.vkDevice.createSwapchainKHR(createInfo);
	}

	void teardown()
	{
		if (swapchain_.has_value())
		{
			swapchainDevice_.value().destroySwapchainKHR(swapchain_.value());
			swapchain_.reset();
		}
	}
	
private:
	std::optional<vk::Device> swapchainDevice_;
	std::optional<vk::SwapchainKHR> swapchain_;
};

