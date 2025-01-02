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


struct BasicCommandQueueTest {
	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<bool>;

	template <typename Row>
	constexpr bainangua::bng_expected<bool> applyRow(Row r) {
		bainangua::VulkanContext& s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		std::vector<vk::CommandBuffer> commandBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("commandBuffers"));
		std::shared_ptr<bainangua::CommandQueueFunnel> graphicsQueue = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsFunnel"));

		vk::CommandBuffer cmd = commandBuffers[0];

		vk::CommandBufferBeginInfo beginInfo({}, {});
		cmd.begin(beginInfo);
		cmd.end();

		vk::SubmitInfo submit(0, nullptr, {}, 1, &cmd, 0, nullptr, nullptr);

		coro::event e;

		auto completionTask = [](coro::event& e) -> coro::task<void> {
			e.set();
			co_return;
			};

		auto awaiterTask = [](const coro::event& e) -> coro::task<void> {
			co_await e;
			co_return;
			};

		graphicsQueue->awaitCommand(submit, completionTask(e));

		coro::sync_wait(awaiterTask(e));

		s.vkDevice.waitIdle();

		return true;
	}
};




TEST_CASE("CommandQueue", "[CommandQueue]")
{
	bainangua::bng_expected<bool> queueTestResult =
		wrapRenderLoopRow("Basic CommandQueue Test",
			bainangua::CreateQueueFunnels()
			| bainangua::SimpleGraphicsCommandPoolStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(1)
			| BasicCommandQueueTest()
		);
	REQUIRE(queueTestResult == bainangua::bng_expected<bool>(true));
}

