#include "expected.hpp" // using tl::expected since this is C++20
#include "RowType.hpp"

#include <algorithm>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>
#include <boost/hana/map.hpp>
#include <boost/hana/hash.hpp>

#include <catch2/catch_test_macros.hpp>
#include <coro/coro.hpp>

#include "nangua_tests.hpp" // this has to be after the coro include, or else wonky double-include occurs...
#include "resourceloader_tests.hpp"

import VulkanContext;
import ResourceLoader;
import Shader;

constexpr auto testLoaderLookup = boost::hana::make_map(
	bainangua::shaderLoader
);

auto testLoaderStorage = bainangua::createLoaderStorage(testLoaderLookup);

using TestResourceLoader = bainangua::ResourceLoader<decltype(testLoaderLookup), decltype(testLoaderStorage)>;

struct BasicShaderTest {
	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<std::string>;

	template <typename Row>
//	requires   RowType::has_named_field<Row, BOOST_HANA_STRING("resourceLoader"), std::shared_ptr<TestResourceLoader>>
	constexpr bainangua::bng_expected<std::string> applyRow(Row r) {
		auto /*std::shared_ptr<TestResourceLoader>*/ loader = boost::hana::at_key(r, BOOST_HANA_STRING("resourceLoader"));

		bainangua::ShaderFileKey key{ std::filesystem::path(SHADER_DIR) / "Basic.vert_spv" };
		bainangua::bng_expected<vk::ShaderModule> result1 = coro::sync_wait(loader->loadResource(key));

		if (!result1) {
			return std::string("shader load failed");
		}

		// there should be one resource loaded (the shader)
		size_t loadedCount = loader->measureLoad();

		// this should unload everything that was loaded
		coro::sync_wait(loader->unloadResource(key));

		// is there anything still loaded? should be 0
		size_t unloadedCount = loader->measureLoad();

		return std::format("shader load success, loaded={} unloaded={}", loadedCount, unloadedCount);
	}
};


TEST_CASE("ResourceLoaderShader", "[ResourceLoader][Shader]")
{
	auto shader_load_test =
		bainangua::QuickCreateContext()
		| bainangua::ResourceLoaderStage(testLoaderLookup, testLoaderStorage)
		| BasicShaderTest();

	REQUIRE(shader_load_test.applyRow(testConfig()) == "shader load success, loaded=1 unloaded=0");
}
