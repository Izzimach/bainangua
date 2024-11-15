

#include "expected.hpp" // using tl::expected since this is C++20
#include "RowType.hpp"

#include <coroutine>
#include <filesystem>
#include <format>
#include <iostream>
#include <boost/hana/map.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/define_struct.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <coro/coro.hpp>

#include "nangua_tests.hpp" // this has to be after the coro include, or else wonky double-include occurs...

import VulkanContext;
import ResourceLoader;

namespace ResourceLoaderTests {

auto testLoaders = boost::hana::make_map(
	boost::hana::make_pair(boost::hana::type_c<bainangua::SingleResourceKey<std::string, int>>, []<typename Resources, typename Storage>(bainangua::ResourceLoader<Resources, Storage>&loader, bainangua::SingleResourceKey<std::string, int> key) -> coro::task<bainangua::bng_expected<int>> {

	std::cout << "int loader running\n";

	auto k1 = bainangua::SingleResourceKey<int, float>{ 1 };
	auto k2 = bainangua::SingleResourceKey<int, float>{ 1 };

	const auto results = co_await coro::when_all(loader.loadResource(k1), loader.loadResource(k2));
	auto result1 = std::get<0>(results).return_value();
	auto result2 = std::get<1>(results).return_value();

	co_return result1.and_then([&](auto xval) {
		return result2.transform([&](auto yval) {
			return static_cast<int>(xval + yval);
			});
		});
}),
	boost::hana::make_pair(boost::hana::type_c<bainangua::SingleResourceKey<int, float>>, []<typename Resources, typename Storage>(bainangua::ResourceLoader<Resources, Storage>& loader, bainangua::SingleResourceKey<int, float> key) -> coro::task<bainangua::bng_expected<float>> {
	std::cout << "float loader running\n";
	co_return bainangua::bng_expected<float>(3.0f + static_cast<float>(key.key));
})
);


struct ResourceLoaderInit {
	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<int, std::string>;

	template<typename Row>
	constexpr tl::expected<int, std::string> applyRow(Row r) {
		bainangua::VulkanContext& s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));

		auto loaderStorage = boost::hana::fold_left(
			testLoaders,
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

		bainangua::bng_expected<bool> result{ true };

		try {
			std::shared_ptr<bainangua::ResourceLoader<decltype(testLoaders), decltype(loaderStorage)>> loader(std::make_shared<bainangua::ResourceLoader<decltype(testLoaders), decltype(loaderStorage)>>(s, testLoaders));
			auto k = bainangua::SingleResourceKey<std::string, int>{ "argh" };
			coro::task<bainangua::bng_expected<int>> loading1 = loader->loadResource(k);
			bainangua::bng_expected<int> result1 = coro::sync_wait(loading1);
			if (result1.has_value()) {
				std::cout << "storage result: " << result1.value() << "\n";
			}
			else {
				std::cout << "result error: " << result1.error() << "\n";
			}

			// this should do cleanup
			coro::sync_wait(loader->unloadResource(k));
		}
		catch (vk::SystemError& err)
		{
			bainangua::bng_errorobject errorString;
			std::format_to(std::back_inserter(errorString), "vk::SystemError: {}", err.what());
			result = tl::make_unexpected(errorString);
		}
		catch (std::exception& err)
		{
			bainangua::bng_errorobject errorString;
			std::format_to(std::back_inserter(errorString), "std::exception: {}", err.what());
			result = tl::make_unexpected(errorString);
		}
		catch (...)
		{
			bainangua::bng_errorobject errorString("unknown error");
			result = tl::make_unexpected(errorString);
		}
		return result;
	}
};


	
TEST_CASE("ResourceLoader", "[ResourceLoader][Basic]")
{
	REQUIRE(
		wrapRenderLoopRow("ResourceLoaderInit", ResourceLoaderInit())
		== bainangua::bng_expected<bool>(true)
	);
}



}