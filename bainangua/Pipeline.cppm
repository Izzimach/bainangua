//
// Code to assemble a Vulkan pipeline.
// 
module;

#include "bainangua.hpp"
#include "expected.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <boost/asio.hpp>
#include <filesystem>
#include <optional>
#include <ranges>
#include <vector>

export module Pipeline;

import PresentationLayer;

namespace bainangua {

std::pmr::vector<char> readFile(std::filesystem::path filePath)
{
	size_t fileSize = std::filesystem::file_size(filePath);

	std::pmr::vector<char> dataBuffer(fileSize);

	boost::asio::io_context io;
	boost::asio::stream_file fileHandle(io, filePath.string(), boost::asio::file_base::flags::read_only);
	size_t completedSize = boost::asio::read(fileHandle, boost::asio::buffer(dataBuffer.data(), fileSize));
	assert(completedSize == fileSize);

	return dataBuffer;
}

vk::ShaderModule createShaderModule(vk::Device device, const std::pmr::vector<char>& shaderBytes)
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
};

template <boost::hana::string ShaderName>
struct CreateShaderModule {
	CreateShaderModule(std::filesystem::path f) : shaderFile(f) {}

	std::filesystem::path shaderFile;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));

		std::pmr::vector<char> shaderCode = readFile(shaderFile);
		vk::ShaderModule shaderModule = createShaderModule(device, shaderCode);
		
		auto rWithShader = boost::hana::insert(r,
			boost::hana::make_pair(ShaderName, shaderModule)
		);
		return f.applyRow(rWithShader);
	}
};

struct CreateBasicRenderPass {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		const PresentationLayer &presentation = boost::hana::at_key(r, BOOST_HANA_STRING("presentation"));

		vk::AttachmentDescription colorAttachment(
			vk::AttachmentDescriptionFlags(),
			presentation.swapChainFormat_,
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

		auto rWithRenderPass = boost::hana::insert(r,
			boost::hana::make_pair(BOOST_HANA_STRING("renderPass"), renderPass)
		);
		return f.applyRow(rWithRenderPass);

	}
};

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

		auto rWithLayout = boost::hana::insert(r,
			boost::hana::make_pair(BOOST_HANA_STRING("layout"), pipelineLayout)
		);

		return f.applyRow(rWithLayout);
	}
};

struct CreateSimplePipeline {
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

		vk::PipelineShaderStageCreateInfo vertexCreateInfo(
			{},
			vk::ShaderStageFlagBits::eVertex,
			vertexShaderModule,
			"main"
		);
		vk::PipelineShaderStageCreateInfo fragmentCreateInfo(
			{},
			vk::ShaderStageFlagBits::eFragment,
			fragmentShaderModule,
			"main"
		);
		vk::PipelineShaderStageCreateInfo shaderStagesInfo[] = { vertexCreateInfo, fragmentCreateInfo };

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo(
			{},
			0, nullptr, // vertex binding
			0, nullptr  // vertex attributes
		);

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo(
			{},
			vk::PrimitiveTopology::eTriangleList,
			false // no restart
		);

		vk::PipelineViewportStateCreateInfo viewportStateInfo(
			{},
			1, nullptr,
			1, nullptr
		);

		vk::PipelineRasterizationStateCreateInfo rasterizerInfo(
			{},
			false,
			false,
			vk::PolygonMode::eFill,
			vk::CullModeFlagBits::eBack,
			vk::FrontFace::eClockwise,
			false, 0, 0, 0,// depth bias
			1.0, // line width
			nullptr // pnext
		);

		vk::PipelineMultisampleStateCreateInfo multiSampleInfo({}, vk::SampleCountFlagBits::e1, false);

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

		vk::DynamicState dynamicStates[] = {
			vk::DynamicState::eViewport,
			vk::DynamicState::eScissor
		};
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
			device.destroyPipelineLayout(pipelineLayout);
			device.destroyShaderModule(vertexShaderModule);
			device.destroyShaderModule(fragmentShaderModule);

			std::string errorMessage;
			fmt::format_to(std::back_inserter(errorMessage), "Failure creating pipeline: {}", vkResultToString(static_cast<VkResult>(result)));
			return tl::make_unexpected(errorMessage);
		}

		auto rWithPipeline = boost::hana::insert(r,
			boost::hana::make_pair(BOOST_HANA_STRING("pipelines"), graphicsPipelines)
			);

		return f.applyRow(rWithPipeline);
	}
};

struct AssemblePipelineBundle {
	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<PipelineBundle, std::string>;

	template<typename Row>
	constexpr tl::expected<PipelineBundle, std::string> applyRow(Row r) {
		std::vector<vk::Pipeline> pipelines = boost::hana::at_key(r, BOOST_HANA_STRING("pipelines"));
		vk::RenderPass renderPass = boost::hana::at_key(r, BOOST_HANA_STRING("renderPass"));
		vk::PipelineLayout pipelineLayout = boost::hana::at_key(r, BOOST_HANA_STRING("layout"));
		vk::ShaderModule vertexShaderModule = boost::hana::at_key(r, BOOST_HANA_STRING("vertexShader"));
		vk::ShaderModule fragmentShaderModule = boost::hana::at_key(r, BOOST_HANA_STRING("fragmentShader"));

		return PipelineBundle{ pipelines, renderPass, pipelineLayout, vertexShaderModule, fragmentShaderModule };
	}
};

export
tl::expected<PipelineBundle, std::string> createPipeline(const PresentationLayer &presentation, std::filesystem::path vertexShaderFile, std::filesystem::path fragmentShaderFile)
{
	vk::Device device = presentation.swapChainDevice_;

	auto pipeRow = boost::hana::make_map(
		boost::hana::make_pair(BOOST_HANA_STRING("device"), device),
		boost::hana::make_pair(BOOST_HANA_STRING("presentation"), presentation)
	);
	auto pipelineChain =
		CreateShaderModule<BOOST_HANA_STRING("vertexShader")>(vertexShaderFile)
		| CreateShaderModule<BOOST_HANA_STRING("fragmentShader")>(fragmentShaderFile)
		| CreateBasicRenderPass()
		| CreateDefaultLayout()
		| CreateSimplePipeline()
		| AssemblePipelineBundle();

	return pipelineChain.applyRow(pipeRow);
}

export
void destroyPipeline(const PresentationLayer &presentation, PipelineBundle& pipeline)
{
	vk::Device device = presentation.swapChainDevice_;

	device.destroyRenderPass(pipeline.renderPass);
	device.destroyPipelineLayout(pipeline.pipelineLayout);
	device.destroyShaderModule(pipeline.vertexShaderModule);
	device.destroyShaderModule(pipeline.fragmentShaderModule);

	std::ranges::for_each(pipeline.graphicsPipelines, [&](vk::Pipeline p) {device.destroyPipeline(p); });
}

}