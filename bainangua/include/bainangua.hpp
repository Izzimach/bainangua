// \file bainangua
// bainangua.hpp : Include files used in most/all of the project .cpp modules

#pragma once

#include <vulkan/vulkan.hpp>
#include "vk_mem_alloc.h"
#include "vk_result_to_string.h"

#include <fmt/format.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <immer/memory_policy.hpp>
#include <immer/array.hpp>
#include <immer/vector.hpp>
#include <expected.hpp>
#include <string_view>

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

	using bng_errorobject = std::pmr::string;

	template <typename V>
	using bng_expected = tl::expected<V, bng_errorobject>;

	template <typename T>
	auto formatVkResultError(std::string_view context, vk::Result result) -> bng_expected<T> {
		bainangua::bng_errorobject errorMessage;
		fmt::format_to(std::back_inserter(errorMessage), "{}: {}", context, vkResultToString(static_cast<VkResult>(result)));
		return tl::make_unexpected(errorMessage);
	}

}