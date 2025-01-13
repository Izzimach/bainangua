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

import VulkanContext;
import ResourceLoader;
import Commands;
import CommandQueue;

template <int loop_count>
auto basicCommandQueueTest = [](auto r) -> bainangua::bng_expected<bool> {
	vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
	std::vector<vk::CommandBuffer> commandBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("commandBuffers"));
	std::shared_ptr<bainangua::CommandQueueFunnel> graphicsFunnel = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsFunnel"));

	vk::CommandBuffer cmd = commandBuffers[0];

	vk::CommandBufferBeginInfo beginInfo({}, {});
	cmd.begin(beginInfo);
	cmd.end();

	vk::SubmitInfo submit(0, nullptr, {}, 1, &cmd, 0, nullptr, nullptr);

	// a single thread to queue onto when we return from the awaitCommand
	coro::thread_pool local_thread{ coro::thread_pool::options{1} };

	auto frameTask = [](const vk::SubmitInfo& submit, std::shared_ptr<bainangua::CommandQueueFunnel> graphicsFunnel, coro::thread_pool &thread) -> coro::task<void> {
		for (unsigned ix = 0; ix < loop_count; ix++) {
			co_await graphicsFunnel->awaitCommand(submit, thread);
		}

		co_return;
	};

	coro::sync_wait(frameTask(submit, graphicsFunnel, local_thread));

	device.waitIdle();

	return true;
};


TEST_CASE("CommandQueue", "[CommandQueue]")
{
	auto program =
		bainangua::QuickCreateContext()
		| bainangua::CreateQueueFunnels()
		| bainangua::SimpleGraphicsCommandPoolStage()
		| bainangua::PrimaryGraphicsCommandBuffersStage(1)
		| RowType::RowWrapLambda<bainangua::bng_expected<bool>>(basicCommandQueueTest<1>);

	REQUIRE(program.applyRow(testConfig()) == bainangua::bng_expected<bool>(true));

	auto program10 =
		bainangua::QuickCreateContext()
		| bainangua::CreateQueueFunnels()
		| bainangua::SimpleGraphicsCommandPoolStage()
		| bainangua::PrimaryGraphicsCommandBuffersStage(1)
		| RowType::RowWrapLambda<bainangua::bng_expected<bool>>(basicCommandQueueTest<10>);

	REQUIRE(program10.applyRow(testConfig()) == bainangua::bng_expected<bool>(true));

}

