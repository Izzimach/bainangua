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
import PerFramePool;
import CommandQueue;

using namespace Catch::Generators;

namespace PerFramePoolTests {

TEST_CASE("PerFramePool", "[PerFramePool][Basic]")
{
	auto perframepool_test =
		bainangua::QuickCreateContext()
		| bainangua::CreateQueueFunnels()
		| bainangua::CreatePerFramePool()
		| RowType::RowWrapLambda<bainangua::bng_expected<std::string>>([](auto row) {
			vk::Device device = boost::hana::at_key(row, BOOST_HANA_STRING("device"));
			std::shared_ptr<bainangua::PerFramePool> perFramePool = boost::hana::at_key(row, BOOST_HANA_STRING("perFramePool"));
			std::shared_ptr<bainangua::CommandQueueFunnel> graphicsQueue = boost::hana::at_key(row, BOOST_HANA_STRING("graphicsFunnel"));

			// a single thread to queue onto when we return from the awaitCommand
			coro::thread_pool local_thread{ coro::thread_pool::options{1} };

			auto runFrame = [](auto perFramePool, auto graphicsQueue, coro::thread_pool &pool) -> coro::task<void> {
				auto pfdResult = co_await perFramePool->acquirePerFrameData();
				if (!pfdResult) { co_return; }
				std::shared_ptr<bainangua::PerFramePool::PerFrameData> pfd = pfdResult.value();

				auto cmdResult = co_await pfd->acquireCommandBuffer();
				if (!cmdResult) { co_return; }
				vk::CommandBuffer cmd = cmdResult.value();

				vk::CommandBufferBeginInfo beginInfo({}, {});
				cmd.begin(beginInfo);
				cmd.end();

				vk::SubmitInfo submit(0, nullptr, {}, 1, &cmd, 0, nullptr, nullptr);

				co_await graphicsQueue->awaitCommand(submit, pool);

				co_await perFramePool->releasePerFrameData(pfd);

				co_return;
				};

			coro::sync_wait(runFrame(perFramePool, graphicsQueue, local_thread));

			device.waitIdle();

			return "PerFramePool success";
		});


	
	REQUIRE(perframepool_test.applyRow(testConfig()) == "PerFramePool success");
}

}