module;

#include "bainangua.hpp"
#include "expected.hpp"
#include "vk_result_to_string.h"


export module DescriptorSets;

import VulkanContext;
import PresentationLayer;

namespace bainangua {

export auto createDescriptorPool(const VulkanContext& s) -> bng_expected<vk::DescriptorPool> {
	vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(bainangua::MultiFrameCount));
	vk::DescriptorPoolCreateInfo poolInfo({}, bainangua::MultiFrameCount, 1, &poolSize);

	vk::DescriptorPool pool;
	auto createResult = s.vkDevice.createDescriptorPool(&poolInfo, nullptr, &pool);
	if (createResult != vk::Result::eSuccess) {
		return formatVkResultError<vk::DescriptorPool>(std::string_view("Error in createDescriptorPool"), createResult);
	}
	return pool;
}

export auto createDescriptorSets(const VulkanContext& s, vk::DescriptorPool pool, vk::DescriptorSetLayout layout) -> bng_expected<std::pmr::vector<vk::DescriptorSet>> {
	uint32_t descriptorSetCount = static_cast<uint32_t>(MultiFrameCount);
	std::pmr::vector<vk::DescriptorSetLayout> layouts(descriptorSetCount, layout);
	vk::DescriptorSetAllocateInfo allocInfo(pool, descriptorSetCount, layouts.data());
	
	
	std::pmr::vector<vk::DescriptorSet> descriptorSets(descriptorSetCount);
	auto allocResult = s.vkDevice.allocateDescriptorSets(&allocInfo, descriptorSets.data());
	if (allocResult != vk::Result::eSuccess)
	{
		return formatVkResultError< std::pmr::vector<vk::DescriptorSet>>(std::string_view("in createDescriptor sets"), allocResult);
	}

	return descriptorSets;
}

}