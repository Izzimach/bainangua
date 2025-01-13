/** 
* Gives you a pool of vulkan resources to pull from for a single frame. Once the frame rendering is completed, you can
* relase all the associated resources at once.
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

export module PerFramePool;

namespace bainangua {

export
class PerFramePool
{
public:
	PerFramePool(vk::Device device, uint32_t queueIndex) : device_(device) {
		vk::CommandPoolCreateInfo info(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queueIndex);
		command_pool_ = device.createCommandPool(info);
	}
	~PerFramePool() {
		device_.destroyCommandPool(command_pool_);
	}

	struct PerFrameData
	{
		PerFrameData(PerFramePool* p) : parent_(p) {}

		PerFramePool* parent_;

		std::vector<vk::Semaphore> semaphores_;
		std::vector<vk::CommandBuffer> command_buffers_;

		auto acquireSemaphore() -> coro::task<bng_expected<vk::Semaphore>> {
			coro::scoped_lock access_lock = co_await parent_->access_mutex_.lock();
			vk::Semaphore s = parent_->device_.createSemaphore(vk::SemaphoreCreateInfo());
			semaphores_.push_back(s);
			co_return s;
		}

		auto acquireCommandBuffer() -> coro::task<bng_expected<vk::CommandBuffer>> {
			coro::scoped_lock access_lock = co_await parent_->access_mutex_.lock();
			if (!parent_->available_command_buffers_.empty()) {
				vk::CommandBuffer c = parent_->available_command_buffers_.back();
				parent_->available_command_buffers_.pop_back();
				co_return c;
			}
			else
			{
				const vk::CommandBufferAllocateInfo info(parent_->command_pool_, vk::CommandBufferLevel::ePrimary, 1);
				std::vector<vk::CommandBuffer> cs = parent_->device_.allocateCommandBuffers(info);
				vk::CommandBuffer c = cs[0];
				command_buffers_.push_back(c);
				co_return c;
			}
		}
	};

	auto acquirePerFrameData() -> coro::task<bng_expected<std::shared_ptr<PerFrameData>>> {
		auto newData = std::make_shared<PerFrameData>(this);

		coro::scoped_lock access_lock = co_await access_mutex_.lock();
		in_use_frame_data_.push_back(newData);
		co_return newData;
	}

	auto releasePerFrameData(std::shared_ptr<PerFrameData> pfd) -> coro::task<bng_expected<void>> {
		coro::scoped_lock access_lock = co_await access_mutex_.lock();
		auto find_pfd = std::ranges::find(in_use_frame_data_, pfd);
		if (find_pfd != in_use_frame_data_.end()) {
			in_use_frame_data_.erase(find_pfd, find_pfd + 1);
			for (auto s : pfd->semaphores_) { device_.destroySemaphore(s); }
			for (auto c : pfd->command_buffers_) { available_command_buffers_.push_back(c); }
			pfd->parent_ = nullptr;
			pfd->semaphores_.clear();
			pfd->command_buffers_.clear();
			co_return {};
		}
		co_return bng_unexpected("PerFramePool: could not find PerFrameData to be released in my in_use_frame_data_");
	}

private:

	vk::Device device_;
	vk::CommandPool command_pool_;
	std::vector<vk::CommandBuffer> available_command_buffers_;

	coro::mutex access_mutex_;

	std::vector<std::shared_ptr<PerFrameData>> in_use_frame_data_;
};


/**
* Creates a PerFramePool
*/
export
struct CreatePerFramePool {

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
		requires   RowType::has_named_field<Row, BOOST_HANA_STRING("device"), vk::Device>
				&& RowType::has_named_field<Row, BOOST_HANA_STRING("graphicsQueueFamilyIndex"), uint32_t>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		uint32_t graphicsQueueIndex = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsQueueFamilyIndex"));

		std::shared_ptr<PerFramePool> pfp = std::make_shared<PerFramePool>(device, graphicsQueueIndex);

		auto rWithPerFramePool = boost::hana::insert(r,
			boost::hana::make_pair(BOOST_HANA_STRING("perFramePool"), pfp)
		);
		return f.applyRow(rWithPerFramePool);
	}
};





}