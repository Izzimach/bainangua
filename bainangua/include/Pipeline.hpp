#pragma once


#include "bainangua.hpp"
#include "PresentationLayer.hpp"

namespace bainangua {

struct PipelineBundle
{
	std::vector<vk::Pipeline> graphicsPipelines;
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::ShaderModule vertexShaderModule;
	vk::ShaderModule fragmentShaderModule;
};

PipelineBundle createPipeline(const PresentationLayer& presentation, const std::string& vertexShaderFile, const std::string& fragmentShaderFile);

void destroyPipeline(const PresentationLayer& presentation, PipelineBundle& pipeline);

}