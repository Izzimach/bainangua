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


template <typename LoaderLookup, typename LoaderStorage>
auto wrapResourceLoader(
	std::string_view name,
	LoaderLookup loaderDirectory,
	std::function<std::string(std::shared_ptr<bainangua::ResourceLoader<LoaderLookup, LoaderStorage>>)> f)
	-> bainangua::bng_expected<std::string>
{
	return wrapRenderLoop<bainangua::bng_expected<std::string>>(name, [&](bainangua::VulkanContext& s) {

		bainangua::bng_expected<std::string> result{ "" };

		try {
			std::shared_ptr<bainangua::ResourceLoader<LoaderLookup, LoaderStorage>>	loader(std::make_shared<bainangua::ResourceLoader<LoaderLookup, LoaderStorage>>(s, loaderDirectory));
			result = f(loader);
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
	});
};


