//
// The presentation layer handles:
// - a (the?) swapchain
// - swapchain images
//

#include <vector>
#include <ranges>
#include <optional>

#include "include/bainangua.hpp"
#include "PresentationLayer.hpp"


namespace {

struct SwapChainProperties
{
	vk::SurfaceCapabilitiesKHR capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR> presentModes;
};

SwapChainProperties querySwapChainProperties(const bainangua::OuterBoilerplateState& boilerplate)
{
	vk::SurfaceCapabilitiesKHR capabilities = boilerplate.vkPhysicalDevice.getSurfaceCapabilitiesKHR(boilerplate.vkSurface);
	std::vector<vk::SurfaceFormatKHR> formats = boilerplate.vkPhysicalDevice.getSurfaceFormatsKHR(boilerplate.vkSurface);
	std::vector<vk::PresentModeKHR> presentModes = boilerplate.vkPhysicalDevice.getSurfacePresentModesKHR(boilerplate.vkSurface);

	return SwapChainProperties(capabilities, formats, presentModes);
}

vk::Extent2D chooseSwapChainImageExtent(const bainangua::OuterBoilerplateState& boilerplate, const SwapChainProperties& swapChainProperties)
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

uint32_t chooseSwapChainImageCount(const SwapChainProperties& swapChainProperties)
{
	const uint32_t minImageCount = swapChainProperties.capabilities.minImageCount;
	const uint32_t maxImageCount = swapChainProperties.capabilities.maxImageCount;
	if (maxImageCount > 0)
	{
		return std::min(minImageCount + 1, maxImageCount);
	}
	return minImageCount + 1;
}
}

namespace bainangua {

void PresentationLayer::build(OuterBoilerplateState& boilerplate)
{
	using namespace std::views;
	SwapChainProperties swapChainInfo = querySwapChainProperties(boilerplate);
	auto useableFormats = swapChainInfo.formats | filter([](vk::SurfaceFormatKHR& s) {return s.format == vk::Format::eB8G8R8A8Srgb && s.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
	auto useableModes = swapChainInfo.presentModes | filter([](vk::PresentModeKHR& s) {return s == vk::PresentModeKHR::eFifo; });

	assert(!useableFormats.empty() && !useableModes.empty());
	swapChainFormat_ = useableFormats.front().format;

	swapChainImageCount_ = chooseSwapChainImageCount(swapChainInfo);
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
	vkGetSwapchainImagesKHR(swapChainDevice_.value(), static_cast<VkSwapchainKHR>(swapChain_.value()), &imageCount, nullptr);
	std::vector<VkImage> swapChainImagesRaw(imageCount);
	vkGetSwapchainImagesKHR(swapChainDevice_.value(), static_cast<VkSwapchainKHR>(swapChain_.value()), &imageCount, swapChainImagesRaw.data());

	// convert the from VkImage to vk::Image
	swapChainImages_.resize(imageCount);
	std::ranges::transform(swapChainImagesRaw, swapChainImages_.begin(),
		[](VkImage v) { return vk::Image(v); });

	swapChainImageViews_.resize(imageCount);
	std::ranges::transform(swapChainImages_, swapChainImageViews_.begin(),
		[&](vk::Image i) {
			vk::ImageViewCreateInfo viewInfo({}, i, vk::ImageViewType::e2D, swapChainFormat_, vk::ComponentMapping(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1), nullptr);
			return swapChainDevice_.value().createImageView(viewInfo);
		});
}

void PresentationLayer::connectRenderPass(vk::RenderPass& renderPass)
{
	teardownFramebuffers();
	swapChainFramebuffers_.resize(swapChainImageCount_);
	std::ranges::transform(swapChainImageViews_, swapChainFramebuffers_.begin(),
		[&](vk::ImageView iv) {
			vk::FramebufferCreateInfo framebufferInfo({}, renderPass, iv, swapChainExtent2D_.width, swapChainExtent2D_.height, 1);
			return swapChainDevice_.value().createFramebuffer(framebufferInfo);
		});
}

void PresentationLayer::teardownFramebuffers()
{
	std::ranges::for_each(swapChainFramebuffers_,
		[&](vk::Framebuffer f) {
			swapChainDevice_.value().destroyFramebuffer(f);
		});
	swapChainFramebuffers_.clear();
}

void PresentationLayer::teardown()
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


}