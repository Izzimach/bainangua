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
* to wait on command completion using a mini-reactor that waits on vkFence objects and calls
* the appropriate coro::task when the fence is signaled.
* 
*/
export
class CommandQueueFunnel
{
public:
	CommandQueueFunnel(vk::Device d, vk::Queue q) : device_(d), queue_(q), fence_awaiter_thread_(std::bind(&CommandQueueFunnel::fence_reactor, this)) {}
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

	// Queue a command but don't immediately wait for it to finish on the GPU. Provides a coro::event that will
	// get signaled when the command finished on the GPU, for you to use later on (or not).
	// Note that after co_await-ing on this coro::event your coroutine will be on the fence_reactor thread,
	// so it's a good idea to immediately requeue onto whatever thread_pool your coroutine was originally on.
	auto asyncCommand(const vk::SubmitInfo& b) -> bng_expected<std::shared_ptr<coro::event>> {
		std::scoped_lock accessLock(access_mutex_);

		bng_expected<vk::Fence> awaitingFence = acquireFence();
		if (!awaitingFence) {
			return bng_unexpected(awaitingFence.error());
		}

		std::shared_ptr<coro::event> event_ptr = std::make_shared<coro::event>();
		queue_.submit(b, awaitingFence.value());
		fence_waiters_.emplace_back(std::make_pair(awaitingFence.value(), event_ptr));

		work_available_.notify_one();
		return event_ptr;
	}
	
	// Submit a command buffer to the queue and provide an awaitable. The coroutine will resume once this command is completed
	// on the GPU (detected using a vkFence). Since this will cause thread jumping
	// you need to also submit a thread_pool where the coroutine will get scheduled once the command buffer is finished.
	auto awaitCommand(const vk::SubmitInfo& b, coro::thread_pool &resume_on) -> coro::task<bng_expected<void>> {

		auto result = asyncCommand(b);
		if (result) {
			co_await (result.value())->operator co_await();
			// because of the way that libcoro events work, at this point we're in the fence reactor thread.
			// so re-queue up the coroutine into it's original thread pool (or at least the thread pool that
			// was passed in!)
			co_await resume_on.schedule();
			co_return{};
		}
		else
			co_return bng_unexpected(result.error());
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

	void fence_reactor() {
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

			vk::Result waitResult = device_.waitForFences(static_cast<uint32_t>(fences.size()), fences.data(), false, 1000); // 1mS timeout
			if (waitResult != vk::Result::eSuccess) {
				// error
			}
			// if any fences have been signaled, remove them from the wait list and run the associated completion task
			lk.lock();
			for (vk::Fence f : fences) {
				if (device_.getFenceStatus(f) == vk::Result::eSuccess) {
					auto waiter = std::find_if(fence_waiters_.begin(), fence_waiters_.end(), [f](auto& x) { return x.first == f; });
					if (waiter != fence_waiters_.end()) {
						// trigger the coro::event used for this fence
						waiter->second->set();

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
	std::vector<std::pair<vk::Fence, std::shared_ptr<coro::event>>> fence_waiters_;
	std::atomic<bool> cleanup_time_{ false };
	std::thread fence_awaiter_thread_;
};

/**
* Creates two CommandQueueFunnels, 'graphicsFunnel' and 'presentFunnel'
*/
export
struct CreateQueueFunnels {

    using row_tag = RowType::RowWrapperTag;

    template <typename WrappedReturnType>
    using return_type_transformer = WrappedReturnType;

    template <typename RowFunction, typename Row>
	requires   RowType::has_named_field<Row, BOOST_HANA_STRING("instance"), vk::Instance>
	        && RowType::has_named_field<Row, BOOST_HANA_STRING("physicalDevice"), vk::PhysicalDevice>
			&& RowType::has_named_field<Row, BOOST_HANA_STRING("device"), vk::Device>
			&& RowType::has_named_field<Row, BOOST_HANA_STRING("graphicsQueue"), vk::Queue>
			&& RowType::has_named_field<Row, BOOST_HANA_STRING("presentQueue"), vk::Queue>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Instance instance = boost::hana::at_key(r, BOOST_HANA_STRING("instance"));
		vk::PhysicalDevice physicalDevice = boost::hana::at_key(r, BOOST_HANA_STRING("physicalDevice"));
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		vk::Queue graphicsQueue = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsQueue"));
		vk::Queue presentQueue = boost::hana::at_key(r, BOOST_HANA_STRING("presentQueue"));
		
		std::shared_ptr<CommandQueueFunnel> graphicsFunnel = std::make_shared<CommandQueueFunnel>(device, graphicsQueue);
		std::shared_ptr<CommandQueueFunnel> presentFunnel =
			graphicsQueue == presentQueue ?
			graphicsFunnel :
			std::make_shared<CommandQueueFunnel>(device, presentQueue);

		auto rWithFunnels = boost::hana::insert(boost::hana::insert(r,
			boost::hana::make_pair(BOOST_HANA_STRING("graphicsFunnel"), graphicsFunnel)),
			boost::hana::make_pair(BOOST_HANA_STRING("presentFunnel"), presentFunnel));
        return f.applyRow(rWithFunnels);
    }
};

}

