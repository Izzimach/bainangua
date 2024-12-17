//
// Code to assemble a Vulkan pipeline.
// 
module;

#include "bainangua.hpp"
#include "expected.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <vector>

export module Pipeline;

import PresentationLayer;
import VertBuffer;
import UniformBuffer;
import DescriptorSets;

namespace bainangua {

std::vector<char> readFile(std::filesystem::path filePath)
{
	size_t fileSize = std::filesystem::file_size(filePath);

	std::vector<char> dataBuffer(fileSize);

	std::fstream fs;
	fs.open(filePath, std::ios_base::binary | std::ios_base::in);
	fs.read(dataBuffer.data(), fileSize);
	std::streamsize readCount = fs.gcount();
	fs.close();

	assert((size_t)readCount == fileSize);

	return dataBuffer;
}

vk::ShaderModule createShaderModule(vk::Device device, const std::vector<char>& shaderBytes)
{
	vk::ShaderModuleCreateInfo createInfo(
		vk::ShaderModuleCreateFlags(),
		shaderBytes.size(),
		reinterpret_cast<const uint32_t*>(shaderBytes.data()),
		nullptr
	);

	vk::ShaderModule module = device.createShaderModule(createInfo);
	return module;
}


export
struct PipelineBundle
{
	std::vector<vk::Pipeline> graphicsPipelines;
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::ShaderModule vertexShaderModule;
	vk::ShaderModule fragmentShaderModule;
	std::optional<vk::DescriptorSetLayout> descriptorLayout;
};

template <typename RowFunction, typename Row>
concept RowExpectsPipeline = requires (RowFunction f, Row r) {
	{ f.applyRow(r) } -> std::convertible_to<tl::expected<PipelineBundle, bng_errorobject>>;
};


export
template <boost::hana::string ShaderName>
struct CreateShaderModule {
	CreateShaderModule(std::filesystem::path f) : shaderFile(f) {}

	std::filesystem::path shaderFile;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
		requires RowExpectsPipeline<RowFunction, Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));

		std::vector<char> shaderCode = readFile(shaderFile);
		vk::ShaderModule shaderModule = createShaderModule(device, shaderCode);
		
		auto rWithShader = boost::hana::insert(r,boost::hana::make_pair(ShaderName, shaderModule));
		tl::expected<PipelineBundle, bng_errorobject> applyResult = f.applyRow(rWithShader);
		if (!applyResult.has_value()) {
			device.destroyShaderModule(shaderModule);
		}
		return applyResult;
	}
};

export
struct CreateBasicRenderPass {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		std::shared_ptr<PresentationLayer> presentation = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));

		vk::AttachmentDescription colorAttachment(
			vk::AttachmentDescriptionFlags(),
			presentation->swapChainFormat_,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		);

		vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::SubpassDescription subpass(
			vk::SubpassDescriptionFlags(),
			vk::PipelineBindPoint::eGraphics,
			0, nullptr, // input attachments
			1, &colorAttachmentRef, // color attachments
			nullptr, // resolve attachments
			nullptr, // depthstencil attachment
			0, nullptr // preserve attachments
		);

		vk::SubpassDependency dependency(
			VK_SUBPASS_EXTERNAL,
			0,
			vk::PipelineStageFlagBits::eColorAttachmentOutput, // srcStageMask
			vk::PipelineStageFlagBits::eColorAttachmentOutput, // dstStageMask
			vk::AccessFlags(), // srcAccessMask
			vk::AccessFlagBits::eColorAttachmentWrite, // dstAccessMask
			{} // dependencyflags
		);
		vk::RenderPassCreateInfo renderPassInfo(
			vk::RenderPassCreateFlags(),
			1, &colorAttachment,
			1, &subpass,
			1, &dependency //dependencies
		);

		vk::RenderPass renderPass = device.createRenderPass(renderPassInfo);

		auto rWithRenderPass = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("renderPass"), renderPass));
		tl::expected<PipelineBundle, bng_errorobject> applyResult = f.applyRow(rWithRenderPass);
		if (!applyResult.has_value()) {
			device.destroyRenderPass(renderPass);
		}
		return applyResult;
	}
};

export
struct CreateDefaultLayout {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
			vk::PipelineLayoutCreateFlags(),
			0, nullptr, // SetLayouts
			0, nullptr // push constants
		);
		vk::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

		auto rWithLayout = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("layout"), pipelineLayout));
		tl::expected<PipelineBundle, bng_errorobject> result = f.applyRow(rWithLayout);
		if (!result.has_value()) {
			device.destroyPipelineLayout(pipelineLayout);
		}
		return result;
	}
};

export
struct CreateMVPDescriptorLayout {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));

		auto layoutResult = createSimpleDescriptorSetLayout(device);
		if (!layoutResult.has_value()) {
			return tl::make_unexpected(layoutResult.error());
		}
		vk::DescriptorSetLayout layout = layoutResult.value();

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
			vk::PipelineLayoutCreateFlags(),
			1, &layout, // SetLayouts
			0, nullptr // push constants
		);
		vk::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

		auto rWithLayout = boost::hana::insert(
			boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("layout"), pipelineLayout)),
			boost::hana::make_pair(BOOST_HANA_STRING("descriptorLayout"), layout)
		);
		return f.applyRow(rWithLayout)
			.or_else([&](bng_errorobject error) {
				device.destroyDescriptorSetLayout(layout);
				device.destroyPipelineLayout(pipelineLayout);
			});
	}
};

export
struct CreateCombinedDescriptorLayout {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));

		auto layoutResult = createCombinedDescriptorSetLayout(device);
		if (!layoutResult.has_value()) {
			return tl::make_unexpected(layoutResult.error());
		}
		vk::DescriptorSetLayout layout = layoutResult.value();

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
			vk::PipelineLayoutCreateFlags(),
			1, &layout, // SetLayouts
			0, nullptr // push constants
		);
		vk::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

		auto rWithLayout = boost::hana::insert(
			boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("layout"), pipelineLayout)),
			boost::hana::make_pair(BOOST_HANA_STRING("descriptorLayout"), layout)
		);
		return f.applyRow(rWithLayout)
			.or_else([&](bng_errorobject error) {
			device.destroyDescriptorSetLayout(layout);
			device.destroyPipelineLayout(pipelineLayout);
				});
	}
};

export
struct CreateNullVertexInfo {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::VertexInputBindingDescription bindingDescription{};
		std::vector<vk::VertexInputAttributeDescription> attributes;
		auto rWithVertexInput = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("vertexInput"), std::make_tuple(bindingDescription, attributes)));
		return f.applyRow(rWithVertexInput);
	}
};

// uses Vertex binding/attributes for the vertex struct from the vulkan tutorial
export
struct CreateVTVertexInfo {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		auto vertexInfo = getVTVertexBindingAndAttributes();
		auto rWithVertexInput = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("vertexInput"), vertexInfo));
		return f.applyRow(rWithVertexInput);
	}

};


export
struct CreateTexVertexInfo {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		auto vertexInfo = getTexVertexBindingAndAttributes();
		auto rWithVertexInput = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("vertexInput"), vertexInfo));
		return f.applyRow(rWithVertexInput);
	}

};



export
struct CreateSimplePipeline {
	CreateSimplePipeline(vk::FrontFace cullMode) : cullMode_(cullMode) { }

	vk::FrontFace cullMode_;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		vk::RenderPass renderPass = boost::hana::at_key(r, BOOST_HANA_STRING("renderPass"));
		vk::PipelineLayout pipelineLayout = boost::hana::at_key(r, BOOST_HANA_STRING("layout"));
		vk::ShaderModule vertexShaderModule = boost::hana::at_key(r, BOOST_HANA_STRING("vertexShader"));
		vk::ShaderModule fragmentShaderModule = boost::hana::at_key(r, BOOST_HANA_STRING("fragmentShader"));
		std::tuple<vk::VertexInputBindingDescription, std::vector<vk::VertexInputAttributeDescription>> vertexInput = boost::hana::at_key(r, BOOST_HANA_STRING("vertexInput"));

		vk::PipelineShaderStageCreateInfo shaderStagesInfo[] = {
			vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main"),
			vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main")
		};
		
		vk::PipelineVertexInputStateCreateInfo vertexInputInfo({},	std::get<0>(vertexInput), std::get<1>(vertexInput));
		
		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo( {}, vk::PrimitiveTopology::eTriangleList, false /* no restart */);

		vk::PipelineViewportStateCreateInfo viewportStateInfo( {}, 1, nullptr, 1, nullptr );

		vk::PipelineRasterizationStateCreateInfo rasterizerInfo(
			{},
			false,
			false,
			vk::PolygonMode::eFill,
			vk::CullModeFlagBits::eBack,
			cullMode_,
			false, 0, 0, 0,// depth bias
			1.0, // line width
			nullptr // pnext
		);

		vk::PipelineMultisampleStateCreateInfo multiSampleInfo( {}, vk::SampleCountFlagBits::e1, false );

		vk::PipelineColorBlendAttachmentState colorBlendAttachment(
			false,
			vk::BlendFactor::eOne, // src
			vk::BlendFactor::eZero, // dst
			vk::BlendOp::eAdd,
			vk::BlendFactor::eOne, // src
			vk::BlendFactor::eZero, // dst
			vk::BlendOp::eAdd,
			(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
		);

		vk::PipelineColorBlendStateCreateInfo colorBlendInfo(
			vk::PipelineColorBlendStateCreateFlags(),
			false,
			vk::LogicOp::eCopy,
			1,
			&colorBlendAttachment,
			std::array<float, 4>{0, 0, 0, 0}
		);

		vk::DynamicState dynamicStates[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicStateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

		vk::GraphicsPipelineCreateInfo pipelineInfo(
			vk::PipelineCreateFlags(),
			shaderStagesInfo,
			&vertexInputInfo,
			&inputAssemblyInfo,
			nullptr,
			&viewportStateInfo,
			&rasterizerInfo,
			&multiSampleInfo,
			nullptr,
			&colorBlendInfo,
			&dynamicStateInfo,
			pipelineLayout,
			renderPass,
			0, // subpass
			VK_NULL_HANDLE,
			-1,
			nullptr
		);
		vk::GraphicsPipelineCreateInfo pipelines[] = { pipelineInfo };
		auto [result, graphicsPipelines] = device.createGraphicsPipelines(VK_NULL_HANDLE, pipelines);

		if (result != vk::Result::eSuccess)
		{
			bainangua::bng_errorobject errorMessage;
			std::format_to(std::back_inserter(errorMessage), "Failure creating pipeline: {}", vkResultToString(static_cast<VkResult>(result)));
			return tl::make_unexpected(errorMessage);
		}

		auto rWithPipeline = boost::hana::insert(r,boost::hana::make_pair(BOOST_HANA_STRING("pipelines"), graphicsPipelines));
		bng_expected<PipelineBundle> applyResult = f.applyRow(rWithPipeline);
		if (!applyResult.has_value()) {
			std::ranges::for_each(graphicsPipelines, [&](vk::Pipeline p) {device.destroyPipeline(p); });
		}
		return applyResult;
	}
};

export
struct AssemblePipelineBundle {
	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<PipelineBundle, bng_errorobject>;

	template<typename Row>
	constexpr bng_expected<PipelineBundle> applyRow(Row r) {
		std::vector<vk::Pipeline> pipelines = boost::hana::at_key(r, BOOST_HANA_STRING("pipelines"));
		vk::RenderPass renderPass = boost::hana::at_key(r, BOOST_HANA_STRING("renderPass"));
		vk::PipelineLayout pipelineLayout = boost::hana::at_key(r, BOOST_HANA_STRING("layout"));
		vk::ShaderModule vertexShaderModule = boost::hana::at_key(r, BOOST_HANA_STRING("vertexShader"));
		vk::ShaderModule fragmentShaderModule = boost::hana::at_key(r, BOOST_HANA_STRING("fragmentShader"));

		std::optional<vk::DescriptorSetLayout> descriptorSetLayout;
		if constexpr (boost::hana::contains(r, BOOST_HANA_STRING("descriptorLayout"))) {
			descriptorSetLayout = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorLayout"));
		}

		return PipelineBundle{ pipelines, renderPass, pipelineLayout, vertexShaderModule, fragmentShaderModule, descriptorSetLayout };
	}
};

export
bng_expected<PipelineBundle> createNoVertexPipeline(std::shared_ptr<PresentationLayer> presentation, std::filesystem::path vertexShaderFile, std::filesystem::path fragmentShaderFile)
{
	vk::Device device = presentation->swapChainDevice_;

	auto pipeRow = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("device"), device),
		boost::hana::make_pair(BOOST_HANA_STRING("presenterptr"), presentation)
	);
	auto pipelineChain =
		CreateShaderModule<BOOST_HANA_STRING("vertexShader")>(vertexShaderFile)
		| CreateShaderModule<BOOST_HANA_STRING("fragmentShader")>(fragmentShaderFile)
		| CreateNullVertexInfo()
		| CreateBasicRenderPass()
		| CreateDefaultLayout()
		| CreateSimplePipeline(vk::FrontFace::eClockwise)
		| AssemblePipelineBundle();

	return pipelineChain.applyRow(pipeRow);
}

export
struct NoVertexPipelineStage {
	NoVertexPipelineStage(std::filesystem::path shaderPath) : shaderPath_(shaderPath) {}

	std::filesystem::path shaderPath_;//

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));

		tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createNoVertexPipeline(presenterptr, (shaderPath_ / "Basic.vert_spv"), (shaderPath_ / "Basic.frag_spv")));
		if (!pipelineResult.has_value()) {
			return tl::make_unexpected(pipelineResult.error());
		}
		bainangua::PipelineBundle pipeline = pipelineResult.value();

		presenterptr->connectRenderPass(pipeline.renderPass);

		auto rWithPipeline = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("pipelineBundle"), pipeline));
		auto result = f.applyRow(rWithPipeline);

		destroyPipeline(context.vkDevice, pipeline);
		return result;
	}
};


export
tl::expected<PipelineBundle, bng_errorobject> createVTVertexPipeline(std::shared_ptr<PresentationLayer> presentation, std::filesystem::path vertexShaderFile, std::filesystem::path fragmentShaderFile)
{
	vk::Device device = presentation->swapChainDevice_;

	auto pipeRow = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("device"), device),
		boost::hana::make_pair(BOOST_HANA_STRING("presenterptr"), presentation)
	);
	auto pipelineChain =
		CreateShaderModule<BOOST_HANA_STRING("vertexShader")>(vertexShaderFile)
		| CreateShaderModule<BOOST_HANA_STRING("fragmentShader")>(fragmentShaderFile)
		| CreateVTVertexInfo()
		| CreateBasicRenderPass()
		| CreateDefaultLayout()
		| CreateSimplePipeline(vk::FrontFace::eClockwise)
		| AssemblePipelineBundle();

	return pipelineChain.applyRow(pipeRow);
}

export
struct VTVertexPipelineStage {
	VTVertexPipelineStage(std::filesystem::path shaderPath) : shaderPath_(shaderPath) {}

	std::filesystem::path shaderPath_;//

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));

		tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createVTVertexPipeline(presenterptr, (shaderPath_ / "PosColor.vert_spv"), (shaderPath_ / "PosColor.frag_spv")));
		if (!pipelineResult.has_value()) {
			return tl::make_unexpected(pipelineResult.error());
		}
		bainangua::PipelineBundle pipeline = pipelineResult.value();

		presenterptr->connectRenderPass(pipeline.renderPass);

		auto rWithPipeline = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("pipelineBundle"), pipeline));
		auto result = f.applyRow(rWithPipeline);

		destroyPipeline(context.vkDevice, pipeline);
		return result;
	}
};



export
tl::expected<PipelineBundle, bng_errorobject> createMVPVertexPipeline(std::shared_ptr<PresentationLayer> presentation, std::filesystem::path vertexShaderFile, std::filesystem::path fragmentShaderFile)
{
	vk::Device device = presentation->swapChainDevice_;

	auto pipeRow = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("device"), device),
		boost::hana::make_pair(BOOST_HANA_STRING("presenterptr"), presentation)
	);
	auto pipelineChain =
		CreateShaderModule<BOOST_HANA_STRING("vertexShader")>(vertexShaderFile)
		| CreateShaderModule<BOOST_HANA_STRING("fragmentShader")>(fragmentShaderFile)
		| CreateVTVertexInfo()
		| CreateBasicRenderPass()
		| CreateMVPDescriptorLayout()
		| CreateSimplePipeline(vk::FrontFace::eCounterClockwise)
		| AssemblePipelineBundle();

	return pipelineChain.applyRow(pipeRow);
}

export
void destroyPipeline(vk::Device device, PipelineBundle& pipeline)
{
	std::ranges::for_each(pipeline.graphicsPipelines, [&](vk::Pipeline p) {device.destroyPipeline(p); });

	device.destroyRenderPass(pipeline.renderPass);
	device.destroyPipelineLayout(pipeline.pipelineLayout);
	if (pipeline.descriptorLayout.has_value()) {
		device.destroyDescriptorSetLayout(pipeline.descriptorLayout.value());
	}
	device.destroyShaderModule(pipeline.vertexShaderModule);
	device.destroyShaderModule(pipeline.fragmentShaderModule);
}

export
struct MVPPipelineStage {
	MVPPipelineStage(std::filesystem::path shaderPath) : shaderPath_(shaderPath) {}

	std::filesystem::path shaderPath_;//

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));

		tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createMVPVertexPipeline(presenterptr, (shaderPath_ / "PosColorMVP.vert_spv"), (shaderPath_ / "PosColor.frag_spv")));
		if (!pipelineResult.has_value()) {
			return tl::make_unexpected(pipelineResult.error());
		}
		bainangua::PipelineBundle pipeline = pipelineResult.value();

		presenterptr->connectRenderPass(pipeline.renderPass);

		auto rWithPipeline = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("pipelineBundle"), pipeline));
		auto result = f.applyRow(rWithPipeline);

		destroyPipeline(context.vkDevice, pipeline);
		return result;
	}
};

export
tl::expected<PipelineBundle, bng_errorobject> createUBOVertexPipeline(std::shared_ptr<PresentationLayer> presentation, std::filesystem::path vertexShaderFile, std::filesystem::path fragmentShaderFile)
{
	vk::Device device = presentation->swapChainDevice_;

	auto pipeRow = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("device"), device),
		boost::hana::make_pair(BOOST_HANA_STRING("presenterptr"), presentation)
	);
	auto pipelineChain =
		CreateShaderModule<BOOST_HANA_STRING("vertexShader")>(vertexShaderFile)
		| CreateShaderModule<BOOST_HANA_STRING("fragmentShader")>(fragmentShaderFile)
		| CreateVTVertexInfo()
		| CreateBasicRenderPass()
		| CreateCombinedDescriptorLayout()
		| CreateSimplePipeline(vk::FrontFace::eCounterClockwise)
		| AssemblePipelineBundle();

	return pipelineChain.applyRow(pipeRow);
}


export
struct UBOPipelineStage {
	UBOPipelineStage(std::filesystem::path shaderPath) : shaderPath_(shaderPath) {}

	std::filesystem::path shaderPath_;//

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));

		tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createUBOVertexPipeline(presenterptr, (shaderPath_ / "PosColorMVP.vert_spv"), (shaderPath_ / "PosColor.frag_spv")));
		if (!pipelineResult.has_value()) {
			return tl::make_unexpected(pipelineResult.error());
		}
		bainangua::PipelineBundle pipeline = pipelineResult.value();

		presenterptr->connectRenderPass(pipeline.renderPass);

		auto rWithPipeline = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("pipelineBundle"), pipeline));
		auto result = f.applyRow(rWithPipeline);

		destroyPipeline(context.vkDevice, pipeline);
		return result;
	}
};





export
tl::expected<PipelineBundle, bng_errorobject> createTexVertexPipeline(std::shared_ptr<PresentationLayer> presentation, std::filesystem::path vertexShaderFile, std::filesystem::path fragmentShaderFile)
{
	vk::Device device = presentation->swapChainDevice_;

	auto pipeRow = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("device"), device),
		boost::hana::make_pair(BOOST_HANA_STRING("presenterptr"), presentation)
	);
	auto pipelineChain =
		CreateShaderModule<BOOST_HANA_STRING("vertexShader")>(vertexShaderFile)
		| CreateShaderModule<BOOST_HANA_STRING("fragmentShader")>(fragmentShaderFile)
		| CreateTexVertexInfo()
		| CreateBasicRenderPass()
		| CreateCombinedDescriptorLayout()
		| CreateSimplePipeline(vk::FrontFace::eCounterClockwise)
		| AssemblePipelineBundle();

	return pipelineChain.applyRow(pipeRow);
}


export
struct TexPipelineStage {
	TexPipelineStage(std::filesystem::path shaderPath) : shaderPath_(shaderPath) {}

	std::filesystem::path shaderPath_;//

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));

		tl::expected<bainangua::PipelineBundle, std::string> pipelineResult(bainangua::createTexVertexPipeline(presenterptr, (shaderPath_ / "TexturedMVP.vert_spv"), (shaderPath_ / "Textured.frag_spv")));
		if (!pipelineResult.has_value()) {
			return tl::make_unexpected(pipelineResult.error());
		}
		bainangua::PipelineBundle pipeline = pipelineResult.value();

		presenterptr->connectRenderPass(pipeline.renderPass);

		auto rWithPipeline = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("pipelineBundle"), pipeline));
		auto result = f.applyRow(rWithPipeline);

		destroyPipeline(context.vkDevice, pipeline);
		return result;
	}
};





}