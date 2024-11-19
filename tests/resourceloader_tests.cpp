

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
#include <boost/hana/define_struct.hpp>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <coro/coro.hpp>

#include "nangua_tests.hpp" // this has to be after the coro include, or else wonky double-include occurs...

import VulkanContext;
import ResourceLoader;

using namespace Catch::Generators;

namespace ResourceLoaderTests {

template <typename Key, typename Resource>
using TestResourceKey = bainangua::SingleResourceKey<Key, Resource>;

// A simple "identity" resource. The loaded resource value is a copy of the key.
template <typename V>
using  IdentityKey = bainangua::SingleResourceKey<V, V>;

// A simple "delay" resource. The key is a float. The loader delays/sleeps for seconds equal to the key, and returns an empty struct.
struct DelayResult {};
using DelayKey = bainangua::SingleResourceKey<float, DelayResult>;

// Recursive chain loader. If the key is N, recursively loads resources with keys N-1 and N-2.
// If N is big this could cause problems, so any key >30 is clamped to 30.
struct ChainLoadResult {};
using ChainLoadKey = bainangua::SingleResourceKey<int, ChainLoadResult>;

// Variable loader. This loads various other resources depending on the value of the key passed in. Used for
// Catch2 generator-based property testing.
struct VariableLoadResult {};
using VariableLoadKey = bainangua::SingleResourceKey<int, VariableLoadResult>;


auto testLoaderLookup = boost::hana::make_map(
	// identity int loader
	boost::hana::make_pair(
		boost::hana::type_c<IdentityKey<int>>, 
		[](auto&loader, IdentityKey<int> key) -> bainangua::LoaderRoutine<int> {
			std::cout << "identity int loader running\n";
			co_return bainangua::LoaderResults{ key.key,std::nullopt };
		}
	),

	// delay loader
	boost::hana::make_pair(
		boost::hana::type_c<DelayKey>, 
		[](auto& loader, DelayKey key) -> bainangua::LoaderRoutine<DelayResult> {
			std::cout << "delay loader running\n";

			// we don't want the delay to be too long, or <0
			float delay = std::clamp(key.key, 0.0f, 10.0f);

			std::this_thread::sleep_for(std::chrono::duration<float>(delay));

			co_return bainangua::LoaderResults{ DelayResult{}, std::nullopt };
		}
	), 

	// chain/recursive loader
	boost::hana::make_pair(
		boost::hana::type_c<ChainLoadKey>,
		[](auto& loader, ChainLoadKey key) -> bainangua::LoaderRoutine<ChainLoadResult> {

			auto clampedKey = std::clamp(key.key, 0, 30);
			std::cout << "chain loader running for " << clampedKey << "\n";

			if (clampedKey > 0) {
				(void)co_await loader.loadResource(ChainLoadKey{ clampedKey - 1 });
				std::cout << "chain loading " << (clampedKey - 1) << "\n";
			}
			if (clampedKey > 1) {
				(void)co_await loader.loadResource(ChainLoadKey{ clampedKey - 2 });
				std::cout << "chain loading " << (clampedKey - 2) << "\n";
			}
			
			co_return bainangua::LoaderResults<ChainLoadResult>{
				ChainLoadResult{},
				// we need to unload dependencies
				[](auto& loader, auto loaded) -> coro::task<bainangua::bng_expected<void>> {
					if (loaded > 0) {
						std::cout << "chain unloading " << (loaded - 1) << "\n";
						co_await (loader.unloadResource(ChainLoadKey{ loaded - 1 }));
					}
					if (loaded > 1) {
						std::cout << "chain unloading " << (loaded - 2) << "\n";
						co_await (loader.unloadResource(ChainLoadKey{ loaded - 2 }));
					}
					co_return bainangua::bng_expected<void>();
				}(loader, clampedKey)
			};
		}
	),

	// variable loader.
	boost::hana::make_pair(
		boost::hana::type_c<VariableLoadKey>,
		[](auto& loader, VariableLoadKey key) -> bainangua::LoaderRoutine<VariableLoadResult> {
			std::optional<IdentityKey<int>> idKey;
			std::optional<DelayKey> delayKey;
			std::optional<ChainLoadKey> chainKey;
			
			// populate (or not) the various keys depending on bits in the VariableLoadKey
			int keyValue = key.key;
			if (keyValue & 1) {
				idKey = IdentityKey<int>(keyValue);
				co_await loader.loadResource(idKey.value());
			}
			if (keyValue & 2) {
				delayKey = DelayKey{ static_cast<float>(keyValue) * 0.1f };
				co_await loader.loadResource(delayKey.value());
			}
			if (keyValue & 4) {
				chainKey = ChainLoadKey{ keyValue };
				co_await loader.loadResource(chainKey.value());
			}

			co_return bainangua::LoaderResults<VariableLoadResult>{
				VariableLoadResult{},
				[](auto& loader, auto keys) -> coro::task<bainangua::bng_expected<void>> {
					if (std::get<0>(keys).has_value()) {
						co_await(loader.unloadResource(std::get<0>(keys).value()));
					}
					if (auto delayKey = std::get<1>(keys).has_value()) {
						co_await(loader.unloadResource(std::get<1>(keys).value()));
					}
					if (auto chainKey = std::get<2>(keys).has_value()) {
						co_await(loader.unloadResource(std::get<2>(keys).value()));
					}
					co_return bainangua::bng_expected<void>();
				}(loader, std::make_tuple(idKey, delayKey, chainKey))
			};
		}
	)
);

auto testLoaderStorage = boost::hana::fold_left(
	testLoaderLookup,
	boost::hana::make_map(),
	[](auto accumulator, auto v) {
		auto HanaKey = boost::hana::first(v);
		using KeyType = typename decltype(HanaKey)::type;
		using ResourceType = typename decltype(HanaKey)::type::resource_type;

		// we'd like to use unique_ptr here but hana forces a copy somewhere internally
		std::unordered_map<KeyType, std::shared_ptr<bainangua::SingleResourceStore<ResourceType>>> storage;

		return boost::hana::insert(accumulator, boost::hana::make_pair(HanaKey, storage));
	}
);

using TestResourceLoader = bainangua::ResourceLoader<decltype(testLoaderLookup), decltype(testLoaderStorage)>;



struct ResourceLoaderInit {

	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<std::string, std::string>;

	std::function<std::string(std::shared_ptr<TestResourceLoader>)> testCode_;

	ResourceLoaderInit(std::function<std::string(std::shared_ptr<TestResourceLoader>)> f) : testCode_(f) {}

	template<typename Row>
	constexpr tl::expected<std::string, std::string> applyRow(Row r) {
		bainangua::VulkanContext& s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));

		bainangua::bng_expected<std::string> result{ "" };

		try {
			std::shared_ptr<TestResourceLoader> loader(std::make_shared<TestResourceLoader>(s, testLoaderLookup));
			result = testCode_(loader);
		}
		catch (vk::SystemError& err)
		{
			result = tl::make_unexpected(std::format("vk::SystemError: {}", err.what()));
		}
		catch (std::exception& err)
		{
			result = tl::make_unexpected(std::format("std::exception: {}", err.what()));
		}
		catch (...)
		{
			result = tl::make_unexpected(std::string("unknown error"));
		}
		return result;
	}
};


	
TEST_CASE("ResourceLoaderBasicLoadUnload", "[ResourceLoader][Basic]")
{
	REQUIRE(
		wrapRenderLoopRow("ResourceLoaderInit", ResourceLoaderInit([](std::shared_ptr<TestResourceLoader> loader) {
			// load a bunch of stuff
			coro::task<bainangua::bng_expected<int>> loading1 = loader->loadResource(IdentityKey<int>(3));
			bainangua::bng_expected<int> result1 = coro::sync_wait(loading1);

			coro::task<bainangua::bng_expected<DelayResult>> loading2 = loader->loadResource(DelayKey(0.5f));
			bainangua::bng_expected<DelayResult> result2 = coro::sync_wait(loading2);

			coro::task<bainangua::bng_expected<ChainLoadResult>> loading3 = loader->loadResource(ChainLoadKey(5));
			bainangua::bng_expected<ChainLoadResult> result3 = coro::sync_wait(loading3);

			// this should do cleanup
			coro::sync_wait(loader->unloadResource(IdentityKey<int>(3)));
			coro::sync_wait(loader->unloadResource(DelayKey(0.5f)));
			coro::sync_wait(loader->unloadResource(ChainLoadKey(5)));

			return std::format("result 1={}", result1.value());
			}
		))
		==
		bainangua::bng_expected<std::string>("result 1=3")
	);
}

TEST_CASE("ResourceLoaderVariableLoad", "[ResourceLoader][Generator]")
{
	auto variableLoad = GENERATE(take(10, random(0, 100)));

	REQUIRE(
		wrapRenderLoopRow("ResourceLoaderInit", ResourceLoaderInit([=](std::shared_ptr<TestResourceLoader> loader) {
			// variable load
			VariableLoadKey key{ variableLoad };
			coro::task<bainangua::bng_expected<VariableLoadResult>> loading1 = loader->loadResource(key);
			bainangua::bng_expected<VariableLoadResult> result1 = coro::sync_wait(loading1);

			// this should unload everything that was loaded
			coro::sync_wait(loader->unloadResource(key));

			// is there anything still loaded?
			size_t usedStorageCount = loader->measureLoad();

			if (result1.has_value()) {
				return std::format("variable load success, load={}", usedStorageCount);
			}
			else {
				return std::string("variable load failed");
			}
		}
		))
		==
		bainangua::bng_expected<std::string>("variable load success, load=0")
	);
}



}