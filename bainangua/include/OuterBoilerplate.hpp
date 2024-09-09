#pragma once

#include <coroutine>

#include "bainangua.hpp"

namespace bainangua {

struct OuterBoilerplateState {
    vk::Instance vkInstance;
    GLFWwindow* glfwWindow;
    vk::PhysicalDevice vkPhysicalDevice;
    vk::Device vkDevice;
    vk::Queue graphicsQueue;
    vk::Queue presentQueue;
    vk::SurfaceKHR vkSurface;

    // your callback should call this at 'end-of-frame'
    std::coroutine_handle<> endOfFrame;
};

struct OuterBoilerplateConfig {
    std::string AppName{ "Vulkan App" };
    std::string EngineName{ "Default Vulkan Engine" };
    std::vector<std::string> requiredExtensions;

    bool useValidation;

    std::function<bool(OuterBoilerplateState&)> innerCode;
};

int outerBoilerplate(const OuterBoilerplateConfig& config);

}