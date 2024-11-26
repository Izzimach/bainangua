/**
* Module for handling command pools.
* The standard CommandPoolKey<Name> lets you choose the type of Name; using std::string is convenient but
* error-prone, so better to define an enum or variant and use that as your Name type.
*/

module;

#include "bainangua.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <boost/container_hash/hash.hpp>
#include <variant>
#include <vector>
#include <coro/coro.hpp>

export module CommandBuffer;

import ResourceLoader;

namespace bainangua {

//
// we define some "standard" pools 
//

// pool to submit to the main graphics queue(s). queueIndex is there in case you have multiple graphics queues.
export struct MainDrawPool { size_t queueIndex; };

export constexpr bool operator==(MainDrawPool const& a, MainDrawPool const& b) {
	return a.queueIndex == b.queueIndex;
}


// pool to submit to a transfer queue.
export struct TransferPool {};

export constexpr bool operator==(TransferPool const&, TransferPool const&) {
	return true;
}


export
using StdCommandPoolKey = SingleResourceKey<std::variant<MainDrawPool, TransferPool>, vk::CommandPool>;

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

export
constexpr std::size_t hash_value(const StdCommandPoolKey& r)
{
	return std::visit(overloaded{
		[](MainDrawPool m) { size_t seed = 0; boost::hash_combine(seed, 1); boost::hash_combine(seed, m.queueIndex); return seed; },
		[](TransferPool) { size_t seed = 0; boost::hash_combine(seed,2); return seed; }
	},
	r.key);
}

export auto commandPoolLoader = boost::hana::make_pair(
	boost::hana::type_c<StdCommandPoolKey>,
	[]<typename Resources, typename Storage>(bainangua::ResourceLoader<Resources, Storage>& loader, StdCommandPoolKey poolkey) -> bainangua::LoaderRoutine<vk::CommandPool> {

		// right now we always use the graphicsQueueFamilyIndex. In the future we may switch to
		// a transfer queue for the TransferQueuePool
		uint32_t queueIndex = std::visit(
			overloaded{
				[&](TransferPool) { return loader.context_.graphicsQueueFamilyIndex; },
				[&](MainDrawPool) { return loader.context_.graphicsQueueFamilyIndex; }
			}, poolkey.key);

		vk::CommandPoolCreateInfo commandPoolInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer,queueIndex);
		vk::CommandPool pool;
		vk::Result result = loader.context_.vkDevice.createCommandPool(&commandPoolInfo, nullptr, &pool);
		if (result != vk::Result::eSuccess) {
			co_return bainangua::bng_unexpected<vk::CommandPool>(std::string("createCommandPool failed in commandPoolLoader"));
		}

		co_return bainangua::bng_expected<bainangua::LoaderResults<vk::CommandPool>>(
			{
				.resource_ = pool,
				.unloader_ = [](vk::Device device, vk::CommandPool p) -> coro::task<bainangua::bng_expected<void>> {
					device.destroyCommandPool(p);
					co_return{};
				}(loader.context_.vkDevice, pool)
			}
		);
	}
);

struct CommandBufferTag {};

// Key to produce a command buffer. Note that
// if you call loader.loadResource() with the same CommandBufferKey, you will get the SAME command buffer. To allocate
// multiple command buffers, call loadResource() with different values of index.
//
export
template <typename PoolKey>
struct CommandBufferKey {
	using hana_key = CommandBufferTag;
	using resource_type = vk::CommandBuffer;

	PoolKey sourcePool;
	bool resettable;
	size_t index;
};

export
template <typename PoolKey>
constexpr bool operator==(const CommandBufferKey<PoolKey>& a, const CommandBufferKey<PoolKey>& b) {
	return a.sourcePool == b.sourcePool
		&& a.resettable == b.resettable
		&& a.index == b.index;
}


export
using StdCommandBufferKey = CommandBufferKey<StdCommandPoolKey>;

export
std::size_t hash_value(const StdCommandBufferKey& r)
{
	size_t seed = 0;
	boost::hash_combine(seed, r.index);
	boost::hash_combine(seed, r.resettable);
	boost::hash_combine(seed, hash_value(r.sourcePool));
	return seed;
}

export
 auto commandBufferLoader = boost::hana::make_pair(
	boost::hana::type_c<StdCommandBufferKey>,
	[]<typename Resources, typename Storage>(bainangua::ResourceLoader<Resources, Storage>& loader, StdCommandBufferKey bufferkey) -> bainangua::LoaderRoutine<vk::CommandBuffer> {
		// first we need to get/allocate the pool
		bng_expected<vk::CommandPool> pool = co_await loader.loadResource(bufferkey.sourcePool);
		if (!pool) {
			co_return bainangua::bng_unexpected<vk::CommandBuffer>(std::string("get commandPool failed in commandBufferLoader"));

		}

		vk::CommandBufferAllocateInfo bufferInfo(pool.value(), vk::CommandBufferLevel::ePrimary, 1);
		vk::CommandBuffer cmd;
		vk::Result result = loader.context_.vkDevice.allocateCommandBuffers(&bufferInfo, &cmd);
		if (result != vk::Result::eSuccess) {
			co_await loader.unloadResource(bufferkey.sourcePool);
			co_return bainangua::bng_unexpected<vk::CommandBuffer>(std::string("allocateCommandBuffers failed in commandBufferLoader"));
		}

		co_return bainangua::bng_expected<bainangua::LoaderResults<vk::CommandBuffer>>(
			{
				.resource_ = cmd,
				.unloader_ = [](auto &loader, vk::CommandPool pool, vk::CommandBuffer cmd, StdCommandPoolKey poolKey) -> coro::task<bainangua::bng_expected<void>> {
					loader.context_.vkDevice.freeCommandBuffers(pool, 1, &cmd);
					co_await loader.unloadResource(poolKey);
					co_return {};
				}(loader, pool.value(), cmd, bufferkey.sourcePool)
			}
		);
	}
);

}

//
// we need a hash function for boost::hana::map
//

export
template <>
struct boost::hana::hash_impl<bainangua::CommandBufferTag> {
	template <typename CommandBufferKey>
	static constexpr auto apply(CommandBufferKey const&) {
		return hana::type_c<CommandBufferKey>;
	}
};

