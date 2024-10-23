//
// This file only exists to instantiate the compilation unit data of "vk_mem_alloc.h" and "vk_result_to_string.h"
//

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#define VK_RESULT_TO_STRING_CONFIG_MAIN
#include "vk_result_to_string.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "bainangua.hpp"
#include "expected.hpp"


#include <vulkan/vulkan.hpp>

namespace bainangua {

	auto formatVkResultError(std::string_view context, vk::Result result) -> tl::unexpected<bng_errorobject> {
		bainangua::bng_errorobject errorMessage;
		std::format_to(std::back_inserter(errorMessage), "{}: {}", context, vkResultToString(static_cast<VkResult>(result)));
		return tl::make_unexpected(errorMessage);
	}

}