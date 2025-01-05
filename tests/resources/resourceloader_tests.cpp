

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

// Bad unloader. This loads an IdentityKey<int> resource but doesn't unload it later. For testing
// if ResourceLoader::measureLoad will actually count the unloaded resource.
struct BadUnloadResult {};
using BadUnloadKey = bainangua::SingleResourceKey<int, BadUnloadResult>;



constexpr auto testLoaderLookup = boost::hana::make_map(
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
			float delay = std::clamp(key.key, 0.0f, 5.0f);

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
				delayKey = DelayKey{ static_cast<float>(keyValue & 0x000f) * 0.1f };
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
	),

	// A bad unloader. Loads an IdentityKey resource but doesn't unload it. With this we check that measureLoad() returns a != 0
	// value when resources are not all unloaded.		
	boost::hana::make_pair(
		boost::hana::type_c<BadUnloadKey>,
		[](auto& loader, BadUnloadKey key) -> bainangua::LoaderRoutine<BadUnloadResult> {
			IdentityKey<int> idKey{ key.key };

			(void)co_await loader.loadResource(idKey);

			co_return bainangua::LoaderResults<BadUnloadResult>{
				BadUnloadResult{},
				[](auto& loader) -> coro::task<bainangua::bng_expected<void>> {

					// normally we would unload the IdentityKey<int> resource here...

					co_return bainangua::bng_expected<void>();
				}(loader)
			};
		}
	)
);

auto testLoaderStorage = bainangua::createLoaderStorage(testLoaderLookup);

using TestResourceLoader = bainangua::ResourceLoader<decltype(testLoaderLookup), decltype(testLoaderStorage)>;

struct BasicLoadTest {
	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<std::string>;

	template <typename Row>
//		requires   RowType::has_named_field<Row, BOOST_HANA_STRING("resourceLoader"), std::shared_ptr<TestResourceLoader>>
	constexpr bainangua::bng_expected<std::string> applyRow(Row r) {
		/*std::shared_ptr<TestResourceLoader>*/ auto loader = boost::hana::at_key(r, BOOST_HANA_STRING("resourceLoader"));

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
};

struct BadUnloadTest {
	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<std::string>;

	template <typename Row>
	//	requires   RowType::has_named_field<Row, BOOST_HANA_STRING("resourceLoader"), std::shared_ptr<TestResourceLoader>>
	constexpr bainangua::bng_expected<std::string> applyRow(Row r) {
		/*std::shared_ptr<TestResourceLoader>*/ auto loader = boost::hana::at_key(r, BOOST_HANA_STRING("resourceLoader"));

		BadUnloadKey key{ 3 };
		// load a bunch of stuff
		coro::task<bainangua::bng_expected<BadUnloadResult>> loading1 = loader->loadResource(key);
		auto result1 = coro::sync_wait(loading1);

		// this should do cleanup of the BadUnload resource but not the dependency
		coro::sync_wait(loader->unloadResource(key));

		size_t usedStorageCount = loader->measureLoad();
		std::cout << "bad unload usedStorageCount=" << usedStorageCount << "\n";

		return std::format("storage count={}", usedStorageCount);
	}
};

TEST_CASE("ResourceLoaderBasicLoadUnload", "[ResourceLoader][Basic]")
{
	auto basic_load_test =
		bainangua::QuickCreateContext()
		| bainangua::ResourceLoaderStage(testLoaderLookup, testLoaderStorage)
		| BasicLoadTest();


	REQUIRE(basic_load_test.applyRow(testConfig()) == "result 1=3");

	auto bad_unload_test = 
		bainangua::QuickCreateContext()
		| bainangua::ResourceLoaderStage(testLoaderLookup, testLoaderStorage)
		| BadUnloadTest();

	REQUIRE(bad_unload_test.applyRow(testConfig()) == "storage count=1");
}


struct VariableLoadTest {
	VariableLoadTest(int k) : variableKey_(k) {}

	int variableKey_;

	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<std::string>;

	template <typename Row>
//		requires   RowType::has_named_field<Row, BOOST_HANA_STRING("resourceLoader"), std::shared_ptr<TestResourceLoader>>
	constexpr bainangua::bng_expected<std::string> applyRow(Row r) {
		/*std::shared_ptr<TestResourceLoader>*/ auto loader = boost::hana::at_key(r, BOOST_HANA_STRING("resourceLoader"));

		// variable load
		VariableLoadKey key{ variableKey_ };
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
};

TEST_CASE("ResourceLoaderVariableLoad", "[ResourceLoader][Generator]")
{
	auto variableKey = GENERATE(take(5, random(0, 100)));

	auto variable_load_test = 
		bainangua::QuickCreateContext()
		| bainangua::ResourceLoaderStage(testLoaderLookup, testLoaderStorage)
		| VariableLoadTest(variableKey);

	REQUIRE(variable_load_test.applyRow(testConfig())	== "variable load success, load=0");
}

}