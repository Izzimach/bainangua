// bainangua.hpp : Include files used in most/all of the project .cpp modules

#pragma once

#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <immer/memory_policy.hpp>

//
// The memory policy to use for immer structures. If not specified,
// the immer templates default to 'immer::default_memory_policy' but
// we may want to switch to something else in the future.
//
using bainangua_memory_policy = immer::default_memory_policy;
