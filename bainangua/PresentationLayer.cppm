//
// The presentation layer handles:
// - a (the?) swapchain
// - swapchain images
//

module;

#include "bainangua.hpp"

#include <immer/array.hpp>
#include <optional>
#include <numeric>
#include <ranges>
#include <vector>

export module PresentationLayer;

import VulkanContext;

struct SwapChainProperties
{
	vk::SurfaceCapabilitiesKHR capabilities;
	immer::array<vk::SurfaceFormatKHR> formats;
	immer::array<vk::PresentModeKHR> presentModes;
};

SwapChainProperties querySwapChainProperties(const bainangua::VulkanContext& boilerplate)
{
	vk::SurfaceCapabilitiesKHR capabilities = boilerplate.vkPhysicalDevice.getSurfaceCapabilitiesKHR(boilerplate.vkSurface);
	std::vector<vk::SurfaceFormatKHR> formats = boilerplate.vkPhysicalDevice.getSurfaceFormatsKHR(boilerplate.vkSurface);
	std::vector<vk::PresentModeKHR> presentModes = boilerplate.vkPhysicalDevice.getSurfacePresentModesKHR(boilerplate.vkSurface);

	return SwapChainProperties(
		capabilities,
		immer::array<vk::SurfaceFormatKHR>(formats.begin(), formats.end()), 
		immer::array<vk::PresentModeKHR>(presentModes.begin(), presentModes.end()));
}

vk::Extent2D chooseSwapChainImageExtent(const bainangua::VulkanContext& boilerplate, const SwapChainProperties& swapChainProperties)
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

namespace bainangua {

export constexpr uint32_t MultiFrameCount = 2;

export struct PresentationLayer
{
	PresentationLayer(
		vk::Device device,
		vk::SwapchainKHR swapChain,
		vk::Format swapChainFormat,
		vk::Extent2D swapChainExtent2D,
		unsigned int swapChainImageCount,
		immer::array<vk::Semaphore, bainangua_memory_policy> imageAvailableSemaphores,
		immer::array<vk::Semaphore, bainangua_memory_policy> renderFinishedSemaphores,
		immer::array<vk::Fence, bainangua_memory_policy> inFlightFences,

		immer::array<vk::Image, bainangua_memory_policy> swapChainImages,
		immer::array<vk::ImageView, bainangua_memory_policy> swapChainImageViews,
		immer::array<vk::Framebuffer, bainangua_memory_policy> swapChainFramebuffers
		) : swapChainDevice_(device), swapChain_(swapChain), swapChainFormat_(swapChainFormat), swapChainExtent2D_(swapChainExtent2D),
	        swapChainImageCount_(swapChainImageCount), imageAvailableSemaphores_(imageAvailableSemaphores), renderFinishedSemaphores_(renderFinishedSemaphores),
		    inFlightFences_(inFlightFences), swapChainImages_(swapChainImages), swapChainImageViews_(swapChainImageViews), swapChainFramebuffers_(swapChainFramebuffers)
			{}
	~PresentationLayer() {}

	void teardown();
	void teardownFramebuffers();

	void connectRenderPass(const vk::RenderPass& renderPass);

	std::optional<PresentationLayer> rebuildSwapChain(VulkanContext& s);

	vk::Device swapChainDevice_;
	vk::SwapchainKHR swapChain_;

	vk::Format swapChainFormat_;
	vk::Extent2D swapChainExtent2D_;
	unsigned int swapChainImageCount_;

	immer::array<vk::Semaphore, bainangua_memory_policy> imageAvailableSemaphores_;
	immer::array<vk::Semaphore, bainangua_memory_policy> renderFinishedSemaphores_;
	immer::array<vk::Fence, bainangua_memory_policy> inFlightFences_;

	immer::array<vk::Image, bainangua_memory_policy> swapChainImages_;
	immer::array<vk::ImageView, bainangua_memory_policy> swapChainImageViews_;
	immer::array<vk::Framebuffer, bainangua_memory_policy> swapChainFramebuffers_;
};


export std::optional<PresentationLayer> buildPresentationLayer(VulkanContext& boilerplate)
{
	SwapChainProperties swapChainInfo = querySwapChainProperties(boilerplate);
	auto useableFormat = std::find_if(swapChainInfo.formats.begin(), swapChainInfo.formats.end(), [](vk::SurfaceFormatKHR s) { return s.format == vk::Format::eB8G8R8A8Srgb && s.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
	auto useableModes = std::find_if(swapChainInfo.presentModes.begin(), swapChainInfo.presentModes.end(), [](vk::PresentModeKHR s) { return s == vk::PresentModeKHR::eFifo; });
	
	assert(useableFormat != swapChainInfo.formats.end());
	assert(useableModes   != swapChainInfo.presentModes.end());

	
	vk::SurfaceFormatKHR swapChainFormat = *useableFormat;

	uint32_t swapChainImageCount = chooseSwapChainImageCount(swapChainInfo);
	vk::Extent2D swapChainExtent2D = chooseSwapChainImageExtent(boilerplate, swapChainInfo);
	std::vector<uint32_t> queueFamilies;

	vk::SwapchainCreateInfoKHR createInfo(
		vk::SwapchainCreateFlagsKHR(),
		boilerplate.vkSurface,
		swapChainImageCount,
		swapChainFormat.format,
		swapChainFormat.colorSpace,
		swapChainExtent2D,
		1,
		vk::ImageUsageFlagBits::eColorAttachment,
		vk::SharingMode::eExclusive,
		queueFamilies,
		swapChainInfo.capabilities.currentTransform,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::PresentModeKHR::eFifo,
		true);
	vk::Device swapChainDevice = boilerplate.vkDevice;
	VkDevice swapChainVkDevice = static_cast<VkDevice>(swapChainDevice);
	auto swapChain = boilerplate.vkDevice.createSwapchainKHR(createInfo);

	// get swapchain images
	uint32_t imageCount;
	VkResult imagesKHRResult;
	imagesKHRResult = vkGetSwapchainImagesKHR(swapChainVkDevice, static_cast<VkSwapchainKHR>(swapChain), &imageCount, nullptr);
	if (imagesKHRResult != VK_SUCCESS) { return std::optional<PresentationLayer>(); }
	std::vector<VkImage> swapChainImagesRaw(imageCount);
	imagesKHRResult = vkGetSwapchainImagesKHR(swapChainVkDevice, static_cast<VkSwapchainKHR>(swapChain), &imageCount, swapChainImagesRaw.data());
	if (imagesKHRResult != VK_SUCCESS) { return std::optional<PresentationLayer>(); }

	// convert the from VkImage to vk::Image
	immer::array<vk::Image, bainangua_memory_policy> swapChainImages = std::accumulate(
		swapChainImagesRaw.begin(), 
		swapChainImagesRaw.end(), 
		immer::array<vk::Image, bainangua_memory_policy>(), 
		[](immer::array<vk::Image> vs, VkImage v) { return vs.push_back(v); });

	immer::array<vk::ImageView, bainangua_memory_policy> swapChainImageViews = std::accumulate(
			swapChainImages.begin(),
			swapChainImages.end(),
			immer::array<vk::ImageView, bainangua_memory_policy>(),
			[&](immer::array<vk::ImageView, bainangua_memory_policy> vs, vk::Image img) {
				vk::ImageViewCreateInfo viewInfo({}, img, vk::ImageViewType::e2D, swapChainFormat.format, vk::ComponentMapping(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1), nullptr);
				vk::ImageView iv =  swapChainDevice.createImageView(viewInfo);
				return vs.push_back(iv);
			});

	immer::array<vk::Semaphore, bainangua_memory_policy> imageAvailableSemaphores;
	immer::array<vk::Semaphore, bainangua_memory_policy> renderFinishedSemaphores;
	immer::array<vk::Fence, bainangua_memory_policy> inFlightFences;

	for (size_t index = 0; index < MultiFrameCount; index++) {
		imageAvailableSemaphores = imageAvailableSemaphores.push_back(swapChainDevice.createSemaphore({}));
		renderFinishedSemaphores = renderFinishedSemaphores.push_back(swapChainDevice.createSemaphore({}));

		vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);
		inFlightFences = inFlightFences.push_back(swapChainDevice.createFence(fenceInfo));
	}
	
	return PresentationLayer(
		swapChainDevice,
		swapChain,

		swapChainFormat.format,
		swapChainExtent2D,

		swapChainImageCount,

		imageAvailableSemaphores,
		renderFinishedSemaphores,
		inFlightFences,

		swapChainImages,
		swapChainImageViews,
		immer::array<vk::Framebuffer, bainangua_memory_policy>()
	);
}

void PresentationLayer::connectRenderPass(const vk::RenderPass& renderPass)
{
	teardownFramebuffers();
	swapChainFramebuffers_ = immer::array<vk::Framebuffer, bainangua_memory_policy>();

	immer::array<vk::Framebuffer, bainangua_memory_policy> swapChainFramebuffers
		= std::accumulate(
			swapChainImageViews_.begin(), 
			swapChainImageViews_.end(), 
			immer::array<vk::Framebuffer, bainangua_memory_policy>(),
			[&](immer::array<vk::Framebuffer, bainangua_memory_policy> vs, vk::ImageView iv) {
				vk::FramebufferCreateInfo framebufferInfo({}, renderPass, 1, &iv, swapChainExtent2D_.width, swapChainExtent2D_.height, 1);
				vk::Framebuffer fb = swapChainDevice_.createFramebuffer(framebufferInfo);
				return vs.push_back(fb);
			});

	swapChainFramebuffers_ = swapChainFramebuffers;
}

void PresentationLayer::teardownFramebuffers()
{
	std::for_each(swapChainFramebuffers_.begin(), swapChainFramebuffers_.end(),
		[&](vk::Framebuffer f) {
			swapChainDevice_.destroyFramebuffer(f);
		});
	swapChainFramebuffers_ = immer::array<vk::Framebuffer, bainangua_memory_policy>();
}

std::optional<PresentationLayer> PresentationLayer::rebuildSwapChain(VulkanContext &s)
{
	s.vkDevice.waitIdle();

	teardown();

	return buildPresentationLayer(s);
}

void PresentationLayer::teardown()
{
	if (swapChainDevice_ && swapChain_)
	{
		std::ranges::for_each(imageAvailableSemaphores_, [&](auto s) { swapChainDevice_.destroySemaphore(s); });
		std::ranges::for_each(renderFinishedSemaphores_, [&](auto s) { swapChainDevice_.destroySemaphore(s); });
		std::ranges::for_each(inFlightFences_,           [&](auto f) { swapChainDevice_.destroyFence(f); });

		teardownFramebuffers();

		std::ranges::for_each(swapChainImageViews_,
			[&](vk::ImageView iv) {
				swapChainDevice_.destroyImageView(iv);
			});
		swapChainImageViews_ = immer::array<vk::ImageView>();
		swapChainDevice_.destroySwapchainKHR(swapChain_);
	}
}

}