/**
* Module for handling command pools and submitting them to queues.
*/

module;

#include "bainangua.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <boost/container_hash/hash.hpp>
#include <variant>
#include <vector>
#include <coro/coro.hpp>
#include <ranges>

export module CommandQueue;

import VulkanContext;

namespace bainangua {

/**
* Provides a controlled channel to use for submitting commands to a queue.
* Multithread access is controlled via a coro mutex. Also provides a coroutine-friendly way
* to wait on command completion.
* 
*/
export
class CommandQueueFunnel
{
public:
	CommandQueueFunnel(vk::Device d, vk::Queue q) : device_(d), queue_(q), fence_awaiter_thread_(std::bind(&CommandQueueFunnel::fence_awaiter, this)) {}
	~CommandQueueFunnel() {
		cleanup_time_ = true;
		work_available_.notify_one();
		fence_awaiter_thread_.join();
		for (auto f : fence_pool_) {
			device_.destroyFence(f);
		}
		// there might have been some waiters that were not completed, make sure to delete those fences as well
		for (auto& w : fence_waiters_) {
			device_.destroyFence(w.first);
		}
	}

	void submitCommand(vk::SubmitInfo &b) {
		std::scoped_lock<std::mutex> accessLock(access_mutex_);
		queue_.submit(b, VK_NULL_HANDLE);
	}

	auto awaitCommand(vk::SubmitInfo& b, coro::task<void> waiter) -> bng_expected<void> {
		{
			std::scoped_lock accessLock(access_mutex_);

			bng_expected<vk::Fence> awaitingFence = acquireFence();
			if (!awaitingFence) {
				return bng_unexpected(awaitingFence.error());
			}
			queue_.submit(b, awaitingFence.value());
			fence_waiters_.emplace_back(std::make_pair(awaitingFence.value(), std::move(waiter)));
		}
		// mutex is unlocked at this point
		work_available_.notify_one();
		return {};
	}

private:
	// MAKE SURE you have the access_mutex_ locked when calling this
	auto acquireFence() -> bng_expected<vk::Fence> {
		if (fence_pool_.empty()) {
			// allocate a new fence - unsignaled
			return device_.createFence(vk::FenceCreateInfo());
		}
		else {
			vk::Fence f = fence_pool_.back();
			vk::Result resetResult = device_.resetFences(1, &f);
			if (resetResult != vk::Result::eSuccess) {
				return bng_unexpected("Error resetting fence");
			}
			fence_pool_.pop_back();
			return f;
		}
	}

	auto releaseFence(vk::Fence releaseme) -> void {
		fence_pool_.push_back(releaseme);
	}

	void fence_awaiter() {
		std::unique_lock lk(access_mutex_);
		std::vector<vk::Fence> fences;

		while (!cleanup_time_) {
			// if there's nothing to do, sleep until something comes up
			if (fence_waiters_.empty() && !cleanup_time_) {
				work_available_.wait(lk, [this] { return !fence_waiters_.empty() || cleanup_time_; });
			}
			if (cleanup_time_) break;

			// get all the fences that we are waiting for
			fences.clear();
			for (auto &p : fence_waiters_) {
				fences.push_back(p.first);
			}
			lk.unlock();

			vk::Result waitResult = device_.waitForFences(fences.size(), fences.data(), false, 1000); // 1mS timeout
			if (waitResult != vk::Result::eSuccess) {
				// error
			}
			// if any fences have been signaled, remove them from the wait list and run the associated completion task
			lk.lock();
			for (vk::Fence f : fences) {
				if (device_.getFenceStatus(f) == vk::Result::eSuccess) {
					auto waiter = std::find_if(fence_waiters_.begin(), fence_waiters_.end(), [f](auto& x) { return x.first == f; });
					if (waiter != fence_waiters_.end()) {
						// run the completion handler
						coro::sync_wait(waiter->second);
						// remove from the vector of waiters
						fence_waiters_.erase(waiter, waiter + 1);
						fence_pool_.push_back(f);
					}
					else
					{
						// error
						std::cerr << "lost completion handler for a fence\n";
					}
				}
			}
		}
	}

	// controls access to the queue, so that multiple threads don't submit at once
	std::mutex access_mutex_;
	std::condition_variable work_available_;

	vk::Device device_;
	vk::Queue queue_;
	std::vector<vk::Fence> fence_pool_;
	std::vector<std::pair<vk::Fence, coro::task<void>>> fence_waiters_;
	bool cleanup_time_{ false };
	std::thread fence_awaiter_thread_;
};

export
struct CreateQueueFunnels {

    using row_tag = RowType::RowWrapperTag;

    template <typename WrappedReturnType>
    using return_type_transformer = WrappedReturnType;

    template <typename RowFunction, typename Row>
    constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
        VulkanContext& context = boost::hana::at_key(r, BOOST_HANA_STRING("context"));

		std::shared_ptr<CommandQueueFunnel> graphicsFunnel = std::make_shared<CommandQueueFunnel>(context.vkDevice, context.graphicsQueue);
		std::shared_ptr<CommandQueueFunnel> presentFunnel =
			context.graphicsQueue == context.presentQueue ?
			graphicsFunnel :
			std::make_shared<CommandQueueFunnel>(context.vkDevice, context.presentQueue);

		auto rWithFunnels = boost::hana::insert(boost::hana::insert(r,
			boost::hana::make_pair(BOOST_HANA_STRING("graphicsFunnel"), graphicsFunnel)),
			boost::hana::make_pair(BOOST_HANA_STRING("presentFunnel"), presentFunnel));
        return f.applyRow(rWithFunnels);
    }
};

}

