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
import CommandQueue;
import PerFramePool;
import Buffers;


TEST_CASE("Buffers", "[Buffers][CPUBuffer][VertexBuffer]")
{
	auto buffer_test =
		bainangua::QuickCreateContext()
		| RowType::RowWrapLambda<bainangua::bng_expected<std::string>>([](auto row) {
			vk::Device device = boost::hana::at_key(row, BOOST_HANA_STRING("device"));
			VmaAllocator vma = boost::hana::at_key(row, BOOST_HANA_STRING("vmaAllocator"));

			auto runFrame = [](VmaAllocator vma) -> coro::task<bainangua::bng_expected<void>> {
				std::array vertData{ 1.0f,2.0f,3.0f };
				auto cpuBuffer = bainangua::allocateStaticHostBuffer(vma, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertData.data(), sizeof(vertData));
				if (cpuBuffer) {
					cpuBuffer.value().release();
					co_return{};
				}
				else {
					co_return bainangua::bng_unexpected(cpuBuffer.error());
				}
			};

			bainangua::bng_expected<void> syncResult = coro::sync_wait(runFrame(vma));

			device.waitIdle();

			return syncResult ? "Buffer success" : syncResult.error();
		});

	REQUIRE(buffer_test.applyRow(testConfig()) == "Buffer success");
}



TEST_CASE("Buffers", "[Buffers][StagingBuffer][VertexBuffer]")
{
	auto buffer_test =
		bainangua::QuickCreateContext()
		| bainangua::CreateQueueFunnels()
		| bainangua::CreatePerFramePool()
		| RowType::RowWrapLambda<bainangua::bng_expected<std::string>>([](auto row) {
			vk::Device device = boost::hana::at_key(row, BOOST_HANA_STRING("device"));
			std::shared_ptr<bainangua::PerFramePool> perFramePool = boost::hana::at_key(row, BOOST_HANA_STRING("perFramePool"));
			std::shared_ptr<bainangua::CommandQueueFunnel> graphicsQueue = boost::hana::at_key(row, BOOST_HANA_STRING("graphicsFunnel"));
			VmaAllocator vma = boost::hana::at_key(row, BOOST_HANA_STRING("vmaAllocator"));

			// a single thread to queue onto when we return from the awaitCommand
			coro::thread_pool local_thread{ coro::thread_pool::options{1} };

			auto runFrame = [](auto perFramePool, auto graphicsQueue, VmaAllocator vma, coro::thread_pool& threads) -> coro::task<bainangua::bng_expected<void>> {
				auto pfdResult = co_await perFramePool->acquirePerFrameData();
				if (!pfdResult) { co_return bainangua::bng_unexpected("failed to acquire PerFrameData"); }
				std::shared_ptr<bainangua::PerFramePool::PerFrameData> pfd = pfdResult.value();

				auto cmdResult = co_await pfd->acquireCommandBuffer();
				if (!cmdResult) { co_return bainangua::bng_unexpected("failed to acquire command buffer"); }
				vk::CommandBuffer cmd = cmdResult.value();

				bainangua::bng_expected<void> returnResult{};

				std::array vertData{ 1.0f,2.0f,3.0f };
				auto stagingBuffer = bainangua::allocateStagingBuffer(vma, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(vertData));
				if (stagingBuffer) {
					auto GPUbuf = co_await bainangua::allocateStaticGPUBuffer(vma, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertData.data(), sizeof(vertData), stagingBuffer.value(), cmd, graphicsQueue, threads);
					if (GPUbuf) {
						GPUbuf.value().release();
					}
					else {
						returnResult = bainangua::bng_unexpected(GPUbuf.error());
					}
					stagingBuffer.value().release();
				}
				else {
					returnResult = bainangua::bng_unexpected(stagingBuffer.error());
				}

				co_await perFramePool->releasePerFrameData(pfd);

				co_return returnResult;
			};

			bainangua::bng_expected<void> syncResult = coro::sync_wait(runFrame(perFramePool, graphicsQueue, vma, local_thread));

			device.waitIdle();

			return syncResult ? "Buffer success" : syncResult.error();
		});

	REQUIRE(buffer_test.applyRow(testConfig()) == "Buffer success");
}
