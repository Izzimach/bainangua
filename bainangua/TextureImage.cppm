module;

#include "bainangua.hpp"
#include "RowType.hpp"

#define STBI_WINDOWS_UTF8
#include "stb_image.h"
#include "vk_mem_alloc.h"

#include <filesystem>

export module TextureImage;

import VulkanContext;
import Commands;

namespace bainangua {

export struct ImageBundle {
	size_t width;
	size_t height;
	size_t channels;
	vk::Image image;
	vk::ImageView imageView;
	VmaAllocation allocation;
};

export auto createTextureImage(const VulkanContext& s, vk::CommandPool pool, std::filesystem::path imagePath) -> bng_expected<ImageBundle> {
	VmaAllocator allocator = s.vmaAllocator;

	int texWidth, texHeight, texChannels;
	std::u8string utf8Path = imagePath.u8string();
	stbi_uc* pixels = stbi_load(reinterpret_cast<const char*>(utf8Path.c_str()), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		return tl::make_unexpected("failed to load texture image!");
	}

	VkDeviceSize bufferSize = texWidth * texHeight * 4;

	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo vmaAllocateInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = 0,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = VK_NULL_HANDLE,
		.pUserData = nullptr,
		.priority = 0.0f
	};
	VkBuffer stagingBuffer;
	VmaAllocation stagingAllocation;
	auto stageResult = vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaAllocateInfo, &stagingBuffer, &stagingAllocation, nullptr);
	if (stageResult != VK_SUCCESS) {
		return tl::make_unexpected("createTextureIndex: vmaCreateBuffer for staging buffer failed");
	}
	VmaAllocationInfo StagingInfo;
	vmaGetAllocationInfo(allocator, stagingAllocation, &StagingInfo);
	if (StagingInfo.pMappedData == nullptr) {
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		return tl::make_unexpected("createTextureIndex: vmaCreateBuffer for staging buffer not mapped");
	}

	memcpy(StagingInfo.pMappedData, pixels, (size_t)bufferSize);

	stbi_image_free(pixels);

	VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_SRGB,
		.extent = VkExtent3D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	VmaAllocationCreateInfo imageVmaAllocateInfo{
		.flags = 0,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	VkImage finalImage;
	VmaAllocation finalAllocation;
	auto imageResult = vmaCreateImage(allocator, &imageCreateInfo, &imageVmaAllocateInfo, &finalImage, &finalAllocation, nullptr);
	if (imageResult != VK_SUCCESS) {
		vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
		return tl::make_unexpected("createTextureIndex: vmaCreateImage for final image failed");
	}

	auto copyResult = submitCommand(s, pool, [&](vk::CommandBuffer buffer) {
		vk::ImageMemoryBarrier copyBarrier(
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			finalImage,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		);
		buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, 0, nullptr, 1, &copyBarrier);

		vk::BufferImageCopy copyRegion(
			0,
			0,
			0,
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
			vk::Offset3D(0,0,0),
			vk::Extent3D(texWidth, texHeight, 1)
		);
		buffer.copyBufferToImage(stagingBuffer, finalImage, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

		vk::ImageMemoryBarrier finalBarrier(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			finalImage,
			VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		);
		buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, 0, nullptr, 0, nullptr, 1, &finalBarrier);
		
		return vk::Result::eSuccess;
	});
	vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

	if (copyResult != vk::Result::eSuccess) {
		vmaDestroyImage(allocator, finalImage, finalAllocation);
		return formatVkResultError("Error copying from staging buffer to final image: {}", copyResult);
	}

	vk::ImageViewCreateInfo viewInfo({}, finalImage, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Srgb, vk::ComponentMapping(), vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1), nullptr);
	vk::ImageView iv = s.vkDevice.createImageView(viewInfo);


	return ImageBundle{
		.width = static_cast<size_t>(texWidth),
		.height = static_cast<size_t>(texHeight),
		.channels = static_cast<size_t>(texChannels),
		.image = finalImage,
		.imageView = iv,
		.allocation = finalAllocation
	};
}

export
auto destroyTextureImage(const VulkanContext& s, ImageBundle image) -> void {
	s.vkDevice.destroyImageView(image.imageView);
	vmaDestroyImage(s.vmaAllocator, image.image, image.allocation);
}

export struct FromFileTextureImageStage {
	FromFileTextureImageStage(std::filesystem::path path) : path_(path) {}

	std::filesystem::path path_;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));

		auto loadResult = createTextureImage(context, commandPool, path_);
		if (!loadResult.has_value()) {
			return tl::make_unexpected(loadResult.error());
		}

		ImageBundle textureImage = loadResult.value();
		auto rWithTextureImage = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("textureImage"), textureImage));
		auto result = f.applyRow(rWithTextureImage);

		bainangua::destroyTextureImage(context, textureImage);

		return result;
	}
};

export auto createTextureSampler(const VulkanContext& s) -> bng_expected<vk::Sampler> {
	vk::PhysicalDeviceProperties physicalProperties;
	s.vkPhysicalDevice.getProperties(&physicalProperties);

	vk::SamplerCreateInfo samplerCreateInfo(
		{},
		vk::Filter::eLinear,
		vk::Filter::eLinear,
		vk::SamplerMipmapMode::eNearest,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		0.0f,
		true,
		physicalProperties.limits.maxSamplerAnisotropy,
		false,
		vk::CompareOp::eAlways,
		0.0f,
		0.0f,
		vk::BorderColor::eIntOpaqueBlack,
		false
	);

	vk::Sampler sampler;
	vk::Result createResult = s.vkDevice.createSampler(&samplerCreateInfo, nullptr, &sampler);
	if (createResult != vk::Result::eSuccess) {
		return bainangua::formatVkResultError("createTextureSampler", createResult);
	}

	return sampler;
}

export auto linkImageToDescriptorSets(const VulkanContext& s, ImageBundle image, vk::Sampler sampler, std::pmr::vector<vk::DescriptorSet> descriptors) -> bng_expected<void> {
	for (auto descriptor : descriptors) {
		vk::DescriptorImageInfo imageInfo(sampler, image.imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::WriteDescriptorSet descriptorWrite(descriptor, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);
		s.vkDevice.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
	}

	return tl::expected<void, bng_errorobject>();
}

export struct Basic2DSamplerStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));

		return createTextureSampler(context)
			.and_then([&](vk::Sampler sampler) {
				auto rWithSampler = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("imageSampler"), sampler));
				auto result = f.applyRow(rWithSampler);

				context.vkDevice.destroySampler(sampler);

				return result;
			});
	}
};

export struct LinkImageToDescriptorsStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::pmr::vector<vk::DescriptorSet> descriptorSets = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorSets"));
		const ImageBundle image = boost::hana::at_key(r, BOOST_HANA_STRING("textureImage"));
		vk::Sampler sampler = boost::hana::at_key(r, BOOST_HANA_STRING("imageSampler"));

		auto linkResult = linkImageToDescriptorSets(context, image, sampler, descriptorSets);
		if (!linkResult) {
			return tl::make_unexpected(linkResult.error());
		}

		return f.applyRow(r);
	}
};



}