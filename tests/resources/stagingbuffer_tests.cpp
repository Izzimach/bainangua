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
import StagingBuffer;


TEST_CASE("ResourceLoaderStagingBuffer", "[ResourceLoader][StagingBuffer][StagingBufferPool]")
{
	constexpr auto testLoaderLookup = boost::hana::make_map(
		bainangua::stagingBufferPoolLoader<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT>,
		bainangua::stagingBufferPoolLoader<VK_BUFFER_USAGE_INDEX_BUFFER_BIT>,
		bainangua::stagingBufferPoolLoader<VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>
	);

	auto testLoaderStorage = bainangua::createLoaderStorage(testLoaderLookup);

	using TestResourceLoader = bainangua::ResourceLoader<decltype(testLoaderLookup), decltype(testLoaderStorage)>;

	auto testWrapper = std::function(wrapResourceLoader<decltype(testLoaderLookup), decltype(testLoaderStorage)>);

	const size_t requestSize = 900;

	SECTION("acquiring a vertex buffer also loads a staging buffer pool") {
		auto testResult =
			testWrapper("ResourceLoaderStagingBuffer", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
				auto buffer = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT>(loader, requestSize));
				if (!buffer) {
					return std::string("acquire staging buffer failed");
				}

				// there should be one pool loaded
				size_t loadedCount = loader->measureLoad();

				// release the buffer - note the pool should unload as well
				coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT>(loader, buffer.value()));

				// is there anything still loaded? should be 0
				size_t unloadedCount = loader->measureLoad();

				return std::format("staging buffer load success, loaded={} unloaded={}", loadedCount, unloadedCount);
			}
			);
		REQUIRE(testResult.has_value());
		REQUIRE(testResult.value() == "staging buffer load success, loaded=1 unloaded=0");
	}

	SECTION("two vertex buffers with the same buffer type share a command pool") {
		auto sharedPoolResult =
			testWrapper("ResourceLoaderStagingBuffer", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
			auto buffer1 = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT>(loader, requestSize));
			auto buffer2 = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT>(loader, requestSize));
			if (!buffer1 || !buffer2) {
					return std::string("acquire staging buffer failed");
				}

				// there should only be one pool loaded
				size_t loadedCount = loader->measureLoad();

				// release the buffers, and the pool shuld also unload
				coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT>(loader, buffer1.value()));
				coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_VERTEX_BUFFER_BIT>(loader, buffer2.value()));

				// is there anything still loaded? should be 0
				size_t unloadedCount = loader->measureLoad();

				return std::format("shared staging buffer load success, loaded={} unloaded={}", loadedCount, unloadedCount);
			}
			);
		REQUIRE(sharedPoolResult.has_value());
		REQUIRE(sharedPoolResult.value() == "shared staging buffer load success, loaded=1 unloaded=0");
	}

	SECTION("acquiring an index buffer also loads a staging buffer pool") {
		auto testResult =
			testWrapper("ResourceLoaderStagingBuffer", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
			auto buffer = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_INDEX_BUFFER_BIT>(loader, requestSize));
			if (!buffer) {
				return std::string("acquire staging buffer failed");
			}

			// there should be one pool loaded
			size_t loadedCount = loader->measureLoad();

			// release the buffer - note the pool should unload as well
			coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_INDEX_BUFFER_BIT>(loader, buffer.value()));

			// is there anything still loaded? should be 0
			size_t unloadedCount = loader->measureLoad();

			return std::format("staging buffer load success, loaded={} unloaded={}", loadedCount, unloadedCount);
				}
			);
		REQUIRE(testResult.has_value());
		REQUIRE(testResult.value() == "staging buffer load success, loaded=1 unloaded=0");
	}

	SECTION("two index buffers with the same buffer type share a command pool") {
		auto sharedPoolResult =
			testWrapper("ResourceLoaderStagingBuffer", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
			auto buffer1 = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_INDEX_BUFFER_BIT>(loader, requestSize));
			auto buffer2 = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_INDEX_BUFFER_BIT>(loader, requestSize));
			if (!buffer1 || !buffer2) {
				return std::string("acquire staging buffer failed");
			}

			// there should only be one pool loaded
			size_t loadedCount = loader->measureLoad();

			// release the buffers, and the pool shuld also unload
			coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_INDEX_BUFFER_BIT>(loader, buffer1.value()));
			coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_INDEX_BUFFER_BIT>(loader, buffer2.value()));

			// is there anything still loaded? should be 0
			size_t unloadedCount = loader->measureLoad();

			return std::format("shared staging buffer load success, loaded={} unloaded={}", loadedCount, unloadedCount);
				}
			);
		REQUIRE(sharedPoolResult.has_value());
		REQUIRE(sharedPoolResult.value() == "shared staging buffer load success, loaded=1 unloaded=0");
	}

	SECTION("acquiring a uniform buffer also loads a staging buffer pool") {
		auto testResult =
			testWrapper("ResourceLoaderStagingBuffer", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
			auto buffer = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>(loader, requestSize));
			if (!buffer) {
				return std::string("acquire staging buffer failed");
			}

			// there should be one pool loaded
			size_t loadedCount = loader->measureLoad();

			// release the buffer - note the pool should unload as well
			coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>(loader, buffer.value()));

			// is there anything still loaded? should be 0
			size_t unloadedCount = loader->measureLoad();

			return std::format("staging buffer load success, loaded={} unloaded={}", loadedCount, unloadedCount);
				}
			);
		REQUIRE(testResult.has_value());
		REQUIRE(testResult.value() == "staging buffer load success, loaded=1 unloaded=0");
	}

	SECTION("two uniform buffers with the same buffer type share a command pool") {
		auto sharedPoolResult =
			testWrapper("ResourceLoaderStagingBuffer", testLoaderLookup, [=](std::shared_ptr<TestResourceLoader> loader) {
			auto buffer1 = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>(loader, requestSize));
			auto buffer2 = coro::sync_wait(bainangua::acquireStagingBuffer<VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>(loader, requestSize));
			if (!buffer1 || !buffer2) {
				return std::string("acquire staging buffer failed");
			}

			// there should only be one pool loaded
			size_t loadedCount = loader->measureLoad();

			// release the buffers, and the pool shuld also unload
			coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>(loader, buffer1.value()));
			coro::sync_wait(releaseStagingBuffer<VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT>(loader, buffer2.value()));

			// is there anything still loaded? should be 0
			size_t unloadedCount = loader->measureLoad();

			return std::format("shared staging buffer load success, loaded={} unloaded={}", loadedCount, unloadedCount);
				}
			);
		REQUIRE(sharedPoolResult.has_value());
		REQUIRE(sharedPoolResult.value() == "shared staging buffer load success, loaded=1 unloaded=0");
	}

}
