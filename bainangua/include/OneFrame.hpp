#pragma once


#include "bainangua.hpp"
#include "Commands.hpp"
#include "OuterBoilerplate.hpp"
#include "Pipeline.hpp"
#include "PresentationLayer.hpp"

namespace bainangua {

vk::Result drawOneFrame(OuterBoilerplateState &s, PresentationLayer &presenter, const PipelineBundle &pipeline, vk::CommandBuffer buffer, size_t multiFrameIndex, std::function<void(vk::CommandBuffer, vk::Framebuffer)> drawCommands);

}

