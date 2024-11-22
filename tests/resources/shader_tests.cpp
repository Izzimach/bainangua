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

auto testWrapper = std::function(wrapResourceLoader<decltype(testLoaderLookup), decltype(testLoaderStorage)>);

TEST_CASE("ResourceLoaderShader", "[ResourceLoader][Shader]")
{
	auto testResult =
		testWrapper("ResourceLoaderShader", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
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
	);
	REQUIRE(testResult == "shader load success, loaded=1 unloaded=0");
}
