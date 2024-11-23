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
import CommandBuffer;

constexpr auto testLoaderLookup = boost::hana::make_map(
	bainangua::commandPoolLoader,
	bainangua::commandBufferLoader
);

auto testLoaderStorage = bainangua::createLoaderStorage(testLoaderLookup);

using TestResourceLoader = bainangua::ResourceLoader<decltype(testLoaderLookup), decltype(testLoaderStorage)>;

auto testWrapper = std::function(wrapResourceLoader<decltype(testLoaderLookup), decltype(testLoaderStorage)>);

TEST_CASE("ResourceLoaderCommandBuffer", "[ResourceLoader][CommandBuffer]")
{
	auto testResult =
		testWrapper("ResourceLoaderCommandBuffer", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
				bainangua::StdCommandBufferKey key{
					.sourcePool = bainangua::StdCommandPoolKey{bainangua::MainDrawPool{loader->context_.graphicsQueueFamilyIndex}},
					.resettable = false,
					.index = 0
				};
				bainangua::bng_expected<vk::CommandBuffer> result1 = coro::sync_wait(loader->loadResource(key));

				if (!result1) {
					return std::string("command buffer load failed");
				}

				// there should be two resources loaded, the command buffer and the command pool
				size_t loadedCount = loader->measureLoad();

				// this should unload everything that was loaded
				coro::sync_wait(loader->unloadResource(key));

				// is there anything still loaded? should be 0
				size_t unloadedCount = loader->measureLoad();

				return std::format("command buffer load success, loaded={} unloaded={}", loadedCount, unloadedCount);
			}
		);
	REQUIRE(testResult == "command buffer load success, loaded=2 unloaded=0");

	auto sharedPoolResult =
		testWrapper("ResourceLoaderCommandBuffer", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
				bainangua::StdCommandPoolKey poolKey(bainangua::MainDrawPool{loader->context_.graphicsQueueFamilyIndex});
				bainangua::StdCommandBufferKey key1{ poolKey, false, 0 };
				bainangua::StdCommandBufferKey key2{ poolKey, false, 1 };

				bainangua::bng_expected<vk::CommandBuffer> result1 = coro::sync_wait(loader->loadResource(key1));
				bainangua::bng_expected<vk::CommandBuffer> result2 = coro::sync_wait(loader->loadResource(key2));

				if (!result1 || !result2) {
					return std::string("command buffer load failed");
				}

				// there should be three resources loaded, the two command buffers and the command pool
				size_t loadedCount = loader->measureLoad();

				// this should unload everything that was loaded
				coro::sync_wait(loader->unloadResource(key1));
				coro::sync_wait(loader->unloadResource(key2));

				// is there anything still loaded? should be 0
				size_t unloadedCount = loader->measureLoad();

				return std::format("shared command pool load success, loaded={} unloaded={}", loadedCount, unloadedCount);
			}
		);
	REQUIRE(sharedPoolResult == "shared command pool load success, loaded=3 unloaded=0");

}
