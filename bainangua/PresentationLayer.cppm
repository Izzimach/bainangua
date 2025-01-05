//
// The presentation layer handles:
// - a (the?) swapchain
// - swapchain images
//

module;

#include "bainangua.hpp"
#include "RowType.hpp"

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
	bainangua::bng_array<vk::SurfaceFormatKHR> formats;
	bainangua::bng_array<vk::PresentModeKHR> presentModes;
};

SwapChainProperties querySwapChainProperties(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
{
	vk::SurfaceCapabilitiesKHR capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	std::vector<vk::SurfaceFormatKHR> formats = physicalDevice.getSurfaceFormatsKHR(surface);
	std::vector<vk::PresentModeKHR> presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

	return SwapChainProperties(
		capabilities,
		bainangua::bng_array<vk::SurfaceFormatKHR>(formats.begin(), formats.end()), 
		bainangua::bng_array<vk::PresentModeKHR>(presentModes.begin(), presentModes.end()));
}

vk::Extent2D chooseSwapChainImageExtent(GLFWwindow *glfwWindow, const SwapChainProperties& swapChainProperties)
{
	if (swapChainProperties.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return swapChainProperties.capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(glfwWindow, &width, &height);

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
		vk::PhysicalDevice physicalDevice,
		vk::SurfaceKHR surface,
		GLFWwindow *window,
		vk::SwapchainKHR swapChain,
		vk::Format swapChainFormat,
		vk::Extent2D swapChainExtent2D,
		unsigned int swapChainImageCount,
		bng_array<vk::Semaphore> imageAvailableSemaphores,
		bng_array<vk::Semaphore> renderFinishedSemaphores,
		bng_array<vk::Fence> inFlightFences,

		bng_array<vk::Image> swapChainImages,
		bng_array<vk::ImageView> swapChainImageViews,
		bng_array<vk::Framebuffer> swapChainFramebuffers
		) : device_(device), physicalDevice_(physicalDevice), surface_(surface), glfwWindow_(window),
		    swapChain_(swapChain), swapChainFormat_(swapChainFormat), swapChainExtent2D_(swapChainExtent2D),
	        swapChainImageCount_(swapChainImageCount), imageAvailableSemaphores_(imageAvailableSemaphores), renderFinishedSemaphores_(renderFinishedSemaphores),
		    inFlightFences_(inFlightFences), swapChainImages_(swapChainImages), swapChainImageViews_(swapChainImageViews), swapChainFramebuffers_(swapChainFramebuffers)
			{}
	~PresentationLayer() { teardown(); }

	void teardown();
	void teardownFramebuffers();

	void connectRenderPass(const vk::RenderPass& renderPass);

	std::shared_ptr<PresentationLayer> rebuildSwapChain();

	// we need all this to rebuild the swapchain
	vk::Device device_;
	vk::PhysicalDevice physicalDevice_;
	vk::SurfaceKHR surface_;
	GLFWwindow* glfwWindow_;

	vk::SwapchainKHR swapChain_;

	vk::Format swapChainFormat_;
	vk::Extent2D swapChainExtent2D_;
	unsigned int swapChainImageCount_;

	bng_array<vk::Semaphore> imageAvailableSemaphores_;
	bng_array<vk::Semaphore> renderFinishedSemaphores_;
	bng_array<vk::Fence> inFlightFences_;

	bng_array<vk::Image> swapChainImages_;
	bng_array<vk::ImageView> swapChainImageViews_;
	bng_array<vk::Framebuffer> swapChainFramebuffers_;
};


export std::shared_ptr<PresentationLayer> buildPresentationLayer(vk::Device device, vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, GLFWwindow *glfwWindow)
{
	SwapChainProperties swapChainInfo = querySwapChainProperties(physicalDevice, surface);
	auto useableFormat = std::find_if(swapChainInfo.formats.begin(), swapChainInfo.formats.end(), [](vk::SurfaceFormatKHR s) { return s.format == vk::Format::eB8G8R8A8Srgb && s.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
	auto useableModes = std::find_if(swapChainInfo.presentModes.begin(), swapChainInfo.presentModes.end(), [](vk::PresentModeKHR s) { return s == vk::PresentModeKHR::eFifo; });
	
	assert(useableFormat != swapChainInfo.formats.end());
	assert(useableModes  != swapChainInfo.presentModes.end());

	vk::SurfaceFormatKHR swapChainFormat = *useableFormat;

	uint32_t swapChainImageCount = chooseSwapChainImageCount(swapChainInfo);
	vk::Extent2D swapChainExtent2D = chooseSwapChainImageExtent(glfwWindow, swapChainInfo);
	std::vector<uint32_t> queueFamilies;

	vk::SwapchainCreateInfoKHR createInfo(
		vk::SwapchainCreateFlagsKHR(),
		surface,
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
	VkDevice swapChainVkDevice = static_cast<VkDevice>(device);
	auto swapChain = device.createSwapchainKHR(createInfo);

	// get swapchain images
	uint32_t imageCount;
	VkResult imagesKHRResult;
	imagesKHRResult = vkGetSwapchainImagesKHR(swapChainVkDevice, static_cast<VkSwapchainKHR>(swapChain), &imageCount, nullptr);
	if (imagesKHRResult != VK_SUCCESS) { return std::shared_ptr<PresentationLayer>(); }
	std::vector<VkImage> swapChainImagesRaw(imageCount);
	imagesKHRResult = vkGetSwapchainImagesKHR(swapChainVkDevice, static_cast<VkSwapchainKHR>(swapChain), &imageCount, swapChainImagesRaw.data());
	if (imagesKHRResult != VK_SUCCESS) { return std::shared_ptr<PresentationLayer>(); }

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
				vk::ImageView iv =  device.createImageView(viewInfo);
				return vs.push_back(iv);
			});

	immer::array<vk::Semaphore, bainangua_memory_policy> imageAvailableSemaphores;
	immer::array<vk::Semaphore, bainangua_memory_policy> renderFinishedSemaphores;
	immer::array<vk::Fence, bainangua_memory_policy> inFlightFences;

	for (size_t index = 0; index < MultiFrameCount; index++) {
		imageAvailableSemaphores = imageAvailableSemaphores.push_back(device.createSemaphore({}));
		renderFinishedSemaphores = renderFinishedSemaphores.push_back(device.createSemaphore({}));

		vk::FenceCreateInfo fenceInfo(vk::FenceCreateFlagBits::eSignaled);
		inFlightFences = inFlightFences.push_back(device.createFence(fenceInfo));
	}
	
	return std::make_shared<PresentationLayer>(
		device,
		physicalDevice,
		surface,
		glfwWindow,
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
	swapChainFramebuffers_ = std::accumulate(
			swapChainImageViews_.begin(), 
			swapChainImageViews_.end(), 
			bng_array<vk::Framebuffer>(),
			[&](auto vs, auto iv) {
				vk::FramebufferCreateInfo framebufferInfo({}, renderPass, 1, &iv, swapChainExtent2D_.width, swapChainExtent2D_.height, 1);
				vk::Framebuffer fb = device_.createFramebuffer(framebufferInfo);
				return vs.push_back(fb);
			});
}

void PresentationLayer::teardownFramebuffers()
{
	std::for_each(swapChainFramebuffers_.begin(), swapChainFramebuffers_.end(),
		[&](vk::Framebuffer f) {
			device_.destroyFramebuffer(f);
		});
	swapChainFramebuffers_ = bng_array<vk::Framebuffer>();
}

std::shared_ptr<PresentationLayer> PresentationLayer::rebuildSwapChain()
{
	device_.waitIdle();

	teardown();

	return buildPresentationLayer(device_, physicalDevice_, surface_, glfwWindow_);
}

void PresentationLayer::teardown()
{
	if (device_ && swapChain_)
	{
		std::ranges::for_each(imageAvailableSemaphores_, [&](auto s) { device_.destroySemaphore(s); });
		std::ranges::for_each(renderFinishedSemaphores_, [&](auto s) { device_.destroySemaphore(s); });
		std::ranges::for_each(inFlightFences_,           [&](auto f) { device_.destroyFence(f); });
		imageAvailableSemaphores_ = bng_array<vk::Semaphore>();
		renderFinishedSemaphores_ = bng_array<vk::Semaphore>();
		inFlightFences_ = bng_array<vk::Fence>();

		teardownFramebuffers();

		std::ranges::for_each(swapChainImageViews_,
			[&](vk::ImageView iv) {
				device_.destroyImageView(iv);
			});
		swapChainImageViews_ = bng_array<vk::ImageView>();

		device_.destroySwapchainKHR(swapChain_);
		swapChain_ = VK_NULL_HANDLE;
	}
}


export struct PresentationLayerStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		vk::PhysicalDevice physicalDevice = boost::hana::at_key(r, BOOST_HANA_STRING("physicalDevice"));
		vk::SurfaceKHR surface = boost::hana::at_key(r, BOOST_HANA_STRING("surface"));
		GLFWwindow* glfwWindow = boost::hana::at_key(r, BOOST_HANA_STRING("glfwWindow"));

		std::shared_ptr<PresentationLayer> presenterptr(buildPresentationLayer(device, physicalDevice, surface, glfwWindow));

		auto rWithPresenter = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("presenterptr"), presenterptr));
		auto result = f.applyRow(rWithPresenter);
		presenterptr->teardown();
		return result;
	}
};

}
