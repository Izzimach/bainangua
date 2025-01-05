module;

#include "bainangua.hpp"
#include "expected.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"


export module DescriptorSets;

import VulkanContext;
import PresentationLayer;

namespace bainangua {

export auto createDescriptorPool(vk::Device device) -> bng_expected<vk::DescriptorPool> {
	vk::DescriptorPoolSize poolSize(vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(bainangua::MultiFrameCount));
	vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, bainangua::MultiFrameCount, 1, &poolSize);

	vk::DescriptorPool pool;
	auto createResult = device.createDescriptorPool(&poolInfo, nullptr, &pool);
	if (createResult != vk::Result::eSuccess) {
		return formatVkResultError(std::string_view("Error in createDescriptorPool"), createResult);
	}
	return pool;
}

export auto createDescriptorSets(vk::Device device, vk::DescriptorPool pool, vk::DescriptorSetLayout layout) -> bng_expected<std::vector<vk::DescriptorSet>> {
	uint32_t descriptorSetCount = static_cast<uint32_t>(MultiFrameCount);
	std::vector<vk::DescriptorSetLayout> layouts(descriptorSetCount, layout);
	vk::DescriptorSetAllocateInfo allocInfo(pool, descriptorSetCount, layouts.data());
	
	
	std::vector<vk::DescriptorSet> descriptorSets(descriptorSetCount);
	auto allocResult = device.allocateDescriptorSets(&allocInfo, descriptorSets.data());
	if (allocResult != vk::Result::eSuccess)
	{
		return formatVkResultError(std::string_view("in createDescriptor sets"), allocResult);
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
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));

		vk::DescriptorPoolSize poolSize(descriptorType_, maxDescriptorCount_);
		vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, maxDescriptorCount_, 1, &poolSize);

		vk::DescriptorPool pool;
		auto createResult = device.createDescriptorPool(&poolInfo, nullptr, &pool);
		if (createResult != vk::Result::eSuccess) {
			return formatVkResultError(std::string_view("Error in CreateSimpleDescriptorPoolStage"), createResult);
		}

		auto rWithPool = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("descriptorPool"), pool));
		auto result = f.applyRow(rWithPool);

		device.destroyDescriptorPool(pool);

		return result;
	}
};

export struct CreateCombinedDescriptorPoolStage {
	CreateCombinedDescriptorPoolStage(uint32_t maxCount) : maxDescriptorCount_(maxCount) {}

	uint32_t maxDescriptorCount_;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));

		std::array<vk::DescriptorPoolSize, 2> poolSizes{
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, maxDescriptorCount_),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, maxDescriptorCount_)

		};
		vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, maxDescriptorCount_, poolSizes);

		vk::DescriptorPool pool;
		auto createResult = device.createDescriptorPool(&poolInfo, nullptr, &pool);
		if (createResult != vk::Result::eSuccess) {
			return formatVkResultError(std::string_view("Error in CreateSimpleDescriptorPoolStage"), createResult);
		}

		auto rWithPool = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("descriptorPool"), pool));
		auto result = f.applyRow(rWithPool);

		device.destroyDescriptorPool(pool);

		return result;
	}
};


export
auto createSimpleDescriptorSetLayout(vk::Device device) -> bng_expected<vk::DescriptorSetLayout> {
	vk::DescriptorSetLayoutBinding uboLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr);

	vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &uboLayoutBinding);
	vk::DescriptorSetLayout descriptorSetLayout;

	vk::Result createResult = device.createDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayout);
	if (createResult == vk::Result::eSuccess) {
		return descriptorSetLayout;
	}
	else {
		std::string errorMessage;
		std::format_to(std::back_inserter(errorMessage), "createSimpleDescriptorSetLayout: could not create descriptor set layout, error={}", vkResultToString((static_cast<VkResult>(createResult))));
		return tl::make_unexpected(errorMessage);
	}
}

export
auto createCombinedDescriptorSetLayout(vk::Device device) -> bng_expected<vk::DescriptorSetLayout> {
	std::vector<vk::DescriptorSetLayoutBinding> bindings{
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
	};
	vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings);
	vk::DescriptorSetLayout descriptorSetLayout;

	vk::Result createResult = device.createDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayout);
	if (createResult == vk::Result::eSuccess) {
		return descriptorSetLayout;
	}
	else {
		return formatVkResultError("CreateCombinedDescriptorSetsStage: could not create descriptor set layout", createResult);
	}
}




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
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		vk::DescriptorPool descriptorPool = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorPool"));

		vk::DescriptorSetLayoutBinding uboLayoutBinding(0, descriptorType_, 1, shaderFlags_, nullptr);
		vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &uboLayoutBinding);
		vk::DescriptorSetLayout descriptorSetLayout;

		vk::Result createResult = device.createDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayout);
		if (createResult != vk::Result::eSuccess) {
			return formatVkResultError("CreateSimpleDescriptorSetsStage: could not create descriptor set layout", createResult);
		}
		std::vector<vk::DescriptorSetLayout> layouts(count_, descriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocInfo(descriptorPool, count_, layouts.data());

		std::vector<vk::DescriptorSet> descriptorSets(count_);
		auto allocResult = device.allocateDescriptorSets(&allocInfo, descriptorSets.data());
		if (allocResult != vk::Result::eSuccess)
		{
			device.destroyDescriptorSetLayout(descriptorSetLayout);
			return formatVkResultError("CreateSimpleDescriptorSetsStage: could not create descriptor sets", allocResult);
		}

		auto rWithSets = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("descriptorSets"), descriptorSets));
		auto result = f.applyRow(rWithSets);

		device.freeDescriptorSets(descriptorPool, count_, descriptorSets.data());
		device.destroyDescriptorSetLayout(descriptorSetLayout);

		return result;
	}
};



export struct CreateCombinedDescriptorSetsStage {
	CreateCombinedDescriptorSetsStage(uint32_t count) :count_(count) {}

	uint32_t count_;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		vk::DescriptorPool descriptorPool = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorPool"));

		auto createResult = createCombinedDescriptorSetLayout(device);
		if (!createResult) {
			return tl::make_unexpected(createResult.error());
		}
		vk::DescriptorSetLayout descriptorSetLayout = createResult.value();

		std::vector<vk::DescriptorSetLayout> layouts(count_, descriptorSetLayout);
		vk::DescriptorSetAllocateInfo allocInfo(descriptorPool, count_, layouts.data());

		std::vector<vk::DescriptorSet> descriptorSets(count_);
		auto allocResult = device.allocateDescriptorSets(&allocInfo, descriptorSets.data());
		if (allocResult != vk::Result::eSuccess)
		{
			device.destroyDescriptorSetLayout(descriptorSetLayout);
			return formatVkResultError("CreateSimpleDescriptorSetsStage: could not create descriptor sets", allocResult);
		}

		auto rWithSets = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("descriptorSets"), descriptorSets));
		auto result = f.applyRow(rWithSets);
	
		device.freeDescriptorSets(descriptorPool, count_, descriptorSets.data());
		device.destroyDescriptorSetLayout(descriptorSetLayout);

		return result;
	}
};


}