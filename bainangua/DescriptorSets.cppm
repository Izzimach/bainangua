module;

#include "bainangua.hpp"
#include "expected.hpp"
#include "RowType.hpp"
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


export struct CreateSimpleDescriptorPoolStage {
	CreateSimpleDescriptorPoolStage(vk::DescriptorType descriptorType, uint32_t maxCount) :
		descriptorType_(descriptorType), maxDescriptorCount_(maxCount) {}

	vk::DescriptorType descriptorType_;
	uint32_t maxDescriptorCount_;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));

		vk::DescriptorPoolSize poolSize(descriptorType_, maxDescriptorCount_);
		vk::DescriptorPoolCreateInfo poolInfo({}, maxDescriptorCount_, 1, &poolSize);

		vk::DescriptorPool pool;
		auto createResult = context.vkDevice.createDescriptorPool(&poolInfo, nullptr, &pool);
		if (createResult != vk::Result::eSuccess) {
			return formatVkResultError<int>(std::string_view("Error in CreateSimpleDescriptorPoolStage"), createResult);
		}

		auto rWithPool = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("descriptorPool"), pool));
		auto result = f.applyRow(rWithPool);

		context.vkDevice.destroyDescriptorPool(pool);

		return result;
	}
};


export struct CreateSimpleDescriptorSetsStage {
	CreateSimpleDescriptorSetsStage(vk::DescriptorType descriptorType, vk::ShaderStageFlags shaderFlags, uint32_t count) :
		descriptorType_(descriptorType), shaderFlags_(shaderFlags), count_(count) {}

	vk::DescriptorType descriptorType_;
	vk::ShaderStageFlags shaderFlags_;
	uint32_t count_;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		vk::DescriptorPool descriptorPool = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorPool"));

		vk::DescriptorSetLayoutBinding uboLayoutBinding(0, descriptorType_, 1, shaderFlags_, nullptr);
		vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &uboLayoutBinding);
		vk::DescriptorSetLayout descriptorSetLayout;

		vk::Result createResult = context.vkDevice.createDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayout);
		if (createResult != vk::Result::eSuccess) {
			return formatVkResultError<int>("CreateSimpleDescriptorSetsStage: could not create descriptor set layout", createResult);
		}
		std::pmr::vector<vk::DescriptorSetLayout> layouts(count_, descriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocInfo(descriptorPool, count_, layouts.data());

		std::pmr::vector<vk::DescriptorSet> descriptorSets(count_);
		auto allocResult = context.vkDevice.allocateDescriptorSets(&allocInfo, descriptorSets.data());
		if (allocResult != vk::Result::eSuccess)
		{
			context.vkDevice.destroyDescriptorSetLayout(descriptorSetLayout);
			return formatVkResultError<int>("CreateSimpleDescriptorSetsStage: could not create descriptor sets", allocResult);
		}

		auto rWithSets = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("descriptorSets"), descriptorSets));
		auto result = f.applyRow(rWithSets);

		context.vkDevice.destroyDescriptorSetLayout(descriptorSetLayout);

		return result;
	}
};

}