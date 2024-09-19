// \file bainangua
// bainangua.hpp : Include files used in most/all of the project .cpp modules

#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <immer/memory_policy.hpp>
#include <immer/array.hpp>
#include <immer/vector.hpp>

namespace bainangua {

	//
	// The memory policy to use for immer structures. If not specified,
	// the immer templates default to 'immer::default_memory_policy' but
	// we may want to switch to something else in the future.
	//
	using bainangua_memory_policy = immer::default_memory_policy;

	template <typename T>
	using bng_array = immer::array<T, bainangua_memory_policy>;

	template <typename T>
	using bng_vector = immer::vector<T, bainangua_memory_policy>;
}