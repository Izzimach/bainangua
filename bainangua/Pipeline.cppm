//
// Code to assemble a Vulkan pipeline.
// 
module;

#include "bainangua.hpp"

#include <boost/asio.hpp>
#include <filesystem>
#include <optional>
#include <ranges>
#include <vector>

export module Pipeline;

import PresentationLayer;

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

namespace bainangua {
export
struct PipelineBundle
{
	std::vector<vk::Pipeline> graphicsPipelines;
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::ShaderModule vertexShaderModule;
	vk::ShaderModule fragmentShaderModule;
};


export
PipelineBundle createPipeline(const PresentationLayer &presentation, std::filesystem::path vertexShaderFile, std::filesystem::path fragmentShaderFile)
{
	vk::Device device = presentation.swapChainDevice_;

	std::pmr::vector<char> vertShaderCode = readFile(vertexShaderFile);
	std::pmr::vector<char> fragShaderCode = readFile(fragmentShaderFile);

	vk::ShaderModule vertexShaderModule = createShaderModule(device, vertShaderCode);
	vk::ShaderModule fragmentShaderModule = createShaderModule(device, fragShaderCode);

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

	vk::DynamicState dynamicStates[] = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
	vk::PipelineDynamicStateCreateInfo dynamicStateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

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

	vk::Viewport viewport(
		0.0f, 0.0f,
		(float)presentation.swapChainExtent2D_.width, (float)presentation.swapChainExtent2D_.height,
		0.0f, 1.0f
	);

	vk::Rect2D scissor({ 0,0 }, presentation.swapChainExtent2D_);

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
		false, 0,0,0,// depth bias
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

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo(
		vk::PipelineLayoutCreateFlags(),
		0, nullptr, // SetLayouts
		0, nullptr // push constants
		);
	vk::PipelineLayout pipelineLayout =	device.createPipelineLayout(pipelineLayoutInfo);


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
	vk::GraphicsPipelineCreateInfo pipelines[] = {pipelineInfo};
	auto [result, graphicsPipelines] = device.createGraphicsPipelines(VK_NULL_HANDLE, pipelines);

	if (result != vk::Result::eSuccess)
	{
		device.destroyPipelineLayout(pipelineLayout);
		device.destroyShaderModule(vertexShaderModule);
		device.destroyShaderModule(fragmentShaderModule);

		throw std::runtime_error("failure building pipeline!");
	}

	return PipelineBundle{ graphicsPipelines, renderPass, pipelineLayout, vertexShaderModule, fragmentShaderModule };
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