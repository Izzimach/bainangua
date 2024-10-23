//
// Code to create and manage uniform buffer objects
//

module;

#include "bainangua.hpp"
#include "expected.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <chrono>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

export module UniformBuffer;

import VulkanContext;
import PresentationLayer; // for MultiFrameCount
import DescriptorSets;

export struct BasicUBO {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};

namespace bainangua {

export struct UniformBufferBundle {
	vk::Buffer ubo;
	VmaAllocation allocation;
	void* mappedMemory;
};

export auto destroyUniformBuffers(VmaAllocator allocator, std::pmr::vector<UniformBufferBundle> buffers) -> void {
	for (auto& b : buffers) {
		vmaDestroyBuffer(allocator, b.ubo, b.allocation);

	}
}

export auto createUniformBuffers(VmaAllocator allocator) -> bng_expected<std::pmr::vector<UniformBufferBundle>> {
	vk::DeviceSize bufferSize = sizeof(BasicUBO);

	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
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

	std::pmr::vector<UniformBufferBundle> uniformBuffers;

	for (size_t i = 0; i < bainangua::MultiFrameCount; i++) {
		VkBuffer buffer;
		VmaAllocation allocation;
		auto vkResult = vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaAllocateInfo, &buffer, &allocation, nullptr);
		if (vkResult != VK_SUCCESS) {
			destroyUniformBuffers(allocator, uniformBuffers);
			return tl::make_unexpected("createUniformBuffers: vmaCreateBuffer for uniform buffer failed");
		}
		VmaAllocationInfo bufferInfo;
		vmaGetAllocationInfo(allocator, allocation, &bufferInfo);
		if (bufferInfo.pMappedData == nullptr) {
			vmaDestroyBuffer(allocator, buffer, allocation);
			destroyUniformBuffers(allocator, uniformBuffers);
			return tl::make_unexpected("createUniformBuffers: vmaCreateBuffer for uniform buffer not mapped");
		}
		uniformBuffers.emplace_back(buffer, allocation, bufferInfo.pMappedData);
	}

	return uniformBuffers;
}

export auto updateUniformBuffer(vk::Extent2D viewportExtent, const UniformBufferBundle& UBOBundle)  -> void {
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	BasicUBO ubo{};
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	float aspectRatio = viewportExtent.width / (float)viewportExtent.height;
	ubo.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 10.0f);
	ubo.projection[1][1] *= -1;

	memcpy(UBOBundle.mappedMemory, &ubo, sizeof(ubo));
}

export auto destroyUniformBuffer(VmaAllocator allocator, UniformBufferBundle UBOBundle) -> void {
	vmaDestroyBuffer(allocator, UBOBundle.ubo, UBOBundle.allocation);
}

export auto linkUBOAndDescriptors(const VulkanContext& s, std::pmr::vector<UniformBufferBundle> ubos, std::pmr::vector<vk::DescriptorSet> descriptors) -> bng_expected<void> {
	if (ubos.size() != descriptors.size()) {
		return tl::make_unexpected("configureUBODescriptors: ubos count does not match descriptor count");
	}

	for (size_t ix = 0; ix < ubos.size(); ix++) {
		vk::DescriptorBufferInfo bufferInfo(ubos[ix].ubo, 0, sizeof(BasicUBO));
		vk::WriteDescriptorSet descriptorWrite(descriptors[ix], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo, nullptr);
		s.vkDevice.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
	}

	return tl::expected<void, bng_errorobject>();
}

export struct CreateAndLinkUniformBuffersStage {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::pmr::vector<vk::DescriptorSet> descriptorSets = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorSets"));

		auto createResult = createUniformBuffers(context.vmaAllocator);
		if (!createResult) {
			return tl::make_unexpected(createResult.error());
		}

		std::pmr::vector<UniformBufferBundle> ubos = createResult.value();
		auto linkResult = linkUBOAndDescriptors(context, ubos, descriptorSets);
		if (!linkResult) {
			destroyUniformBuffers(context.vmaAllocator, ubos);
			return tl::make_unexpected(linkResult.error());
		}

		auto rWithUBOs = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("uniformBuffers"), ubos));
		auto result = f.applyRow(rWithUBOs);

		destroyUniformBuffers(context.vmaAllocator, ubos);

		return result;

	}
};

}