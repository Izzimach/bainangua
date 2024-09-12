#pragma once


#include "bainangua.hpp"
#include "PresentationLayer.hpp"

#include <filesystem>

namespace bainangua {

struct PipelineBundle
{
	std::vector<vk::Pipeline> graphicsPipelines;
	vk::RenderPass renderPass;
	vk::PipelineLayout pipelineLayout;
	vk::ShaderModule vertexShaderModule;
	vk::ShaderModule fragmentShaderModule;
};

PipelineBundle createPipeline(const PresentationLayer& presentation, std::filesystem::path vertexShaderFile, std::filesystem::path fragmentShaderFile);

void destroyPipeline(const PresentationLayer& presentation, PipelineBundle& pipeline);

}