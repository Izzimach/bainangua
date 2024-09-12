#pragma once


#include "bainangua.hpp"
#include "Commands.hpp"
#include "OuterBoilerplate.hpp"
#include "PresentationLayer.hpp"

namespace bainangua {

vk::Result drawOneFrame(const OuterBoilerplateState &s, const PresentationLayer &presenter, vk::CommandBuffer buffer, std::function<void(vk::CommandBuffer, vk::Framebuffer)> drawCommands);

}

