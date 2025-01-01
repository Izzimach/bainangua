module;

#include "bainangua.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <variant>
#include <vector>
#include <map>
#include <string_view>
#include <coro/coro.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/define_struct.hpp>


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <reflect.hpp>

export module VertexBuffer;

namespace bainangua {

// need this to use ""sv literal

/**
* We try to deduce vertex format from the name of each vertex field.
*/
using namespace std::literals;

// we map from member name to vertex format. Also includes the expected size of the data as an additional check.
constexpr std::array vertexTypeMap{
	std::make_tuple("pos3"sv, vk::Format::eR32G32B32Sfloat, (size_t)12),
	std::make_tuple("pos4"sv, vk::Format::eR32G32B32A32Sfloat, (size_t)16),
	std::make_tuple("normal3"sv, vk::Format::eR32G32B32Sfloat, (size_t)12),
	std::make_tuple("uv"sv, vk::Format::eR32G32Sfloat, (size_t)8),
	std::make_tuple("uvw"sv, vk::Format::eR32G32B32Sfloat, (size_t)12)
};
	
[[nodiscard]] constexpr auto lookupVertexType(const std::string_view identifier) -> std::tuple<std::string_view, vk::Format, size_t> {
	for (auto const &vpair : vertexTypeMap) {
		auto &[k, v, e] = vpair;
		if (k == identifier) {
			return vpair;
		}
	}
	throw std::range_error("vertex type not found");
}


template <typename VertexStruct, size_t ArraySize>
constexpr auto attributes(const VertexStruct v) -> std::array<vk::VertexInputAttributeDescription, ArraySize>  {
	std::array<vk::VertexInputAttributeDescription, ArraySize> attributes;

	reflect::for_each([&attributes](auto I) {
			std::string_view fieldName = reflect::member_name<I, VertexStruct>();
			size_t actualSize = reflect::size_of<I, VertexStruct>();

			auto [name, format, expectedSize] = lookupVertexType(fieldName);
			assert(expectedSize == actualSize);

			attributes[I] = vk::VertexInputAttributeDescription(static_cast<uint32_t>(I), 0, format, static_cast<uint32_t>(reflect::offset_of<I, VertexStruct>()));
		}, v);

	return attributes;
}

/**
* Generate vertex attributes for a given C++ struct. In order to have the attribute formats deduced, you need to use
* a standard naming scheme for the data members.  Look at bainangua::vertexType::vertexTypeMap
*/
export
template <typename VertexStruct>
constexpr auto reflectAttributes(const VertexStruct v = {}) {
	return attributes<VertexStruct, reflect::size<VertexStruct>()>(v);
}


}