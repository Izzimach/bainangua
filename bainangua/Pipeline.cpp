//
// Code to assemble a Vulkan pipeline.
// 


#include <vector>
#include <ranges>
#include <optional>
#include <filesystem>
#include <boost/asio.hpp>

#include "bainangua.hpp"
#include "Pipeline.hpp"

namespace {

std::vector<char> readFile(const std::string& fileName)
{
	std::filesystem::path filePath(fileName);
	size_t fileSize = std::filesystem::file_size(filePath);

	std::vector<char> dataBuffer(fileSize);

	boost::asio::io_context io;
	boost::asio::stream_file fileHandle(io, fileName, boost::asio::file_base::flags::read_only);
	size_t completedSize = boost::asio::read(fileHandle, boost::asio::buffer(dataBuffer.data(), fileSize));
	assert(completedSize == fileSize);

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

}

namespace bainangua {

PipelineBundle createPipeline(const PresentationLayer &presentation, const std::string& vertexShaderFile, const std::string& fragmentShaderFile)
{
	vk::Device device = presentation.swapChainDevice_.value();

	std::vector<char> vertShaderCode = readFile(vertexShaderFile);
	std::vector<char> fragShaderCode = readFile(fragmentShaderFile);

	vk::ShaderModule vertexShaderModule = createShaderModule(device, vertShaderCode);
	vk::ShaderModule fragmentShaderModule = createShaderModule(device, fragShaderCode);

	vk::PipelineShaderStageCreateInfo vertCreateInfo(
		{},
		vk::ShaderStageFlagBits::eVertex,
		vertexShaderModule,
		"main"
	);
	vk::PipelineShaderStageCreateInfo fragCreateInfo(
		{},
		vk::ShaderStageFlagBits::eFragment,
		fragmentShaderModule,
		"main"
	);
	vk::PipelineShaderStageCreateInfo shaderStagesInfo[] = { vertCreateInfo, fragCreateInfo };

	std::vector<vk::DynamicState> dynamicStates = {
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
		false // ignore depth bias and lineWidth
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

	vk::RenderPassCreateInfo renderPassInfo(
		vk::RenderPassCreateFlags(),
		1, &colorAttachment,
		1, &subpass,
		0, nullptr //dependencies
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
	std::array<vk::GraphicsPipelineCreateInfo, 1> pipelines = { pipelineInfo };
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

void destroyPipeline(const PresentationLayer &presentation, PipelineBundle& pipeline)
{
	vk::Device device = presentation.swapChainDevice_.value();

	device.destroyRenderPass(pipeline.renderPass);
	device.destroyPipelineLayout(pipeline.pipelineLayout);
	device.destroyShaderModule(pipeline.vertexShaderModule);
	device.destroyShaderModule(pipeline.fragmentShaderModule);

	std::ranges::for_each(pipeline.graphicsPipelines, [&](vk::Pipeline p) {device.destroyPipeline(p); });
}

}