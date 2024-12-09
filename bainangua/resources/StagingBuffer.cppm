/**
* Module for handling staging buffers.
* To get buffer data to GPU-side memory we normally dump it into a CPU-side staging buffer and then use a transfer command to copy the data into a
* GPU-side buffer.
* 
* We don't want to allocate/deallocate a staging buffer every time data needs to be transferred; instead we keep a pool of
* a few staging buffers. When someone needs a staging buffer they specify the size and then wait until a buffer of adequate size
* becomes available.
*/

module;

#include "bainangua.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <boost/container_hash/hash.hpp>
#include <variant>
#include <vector>
#include <coro/coro.hpp>


export module StagingBuffer;

import ResourceLoader;

namespace bainangua {

export
struct StagingBufferInfo {
	size_t sizeInBytes;
	void* mappedData;
	vk::Buffer buffer;
	VmaAllocation vmaInfo;
};

auto createStagingBuffer(VmaAllocator allocator, VkBufferUsageFlags usageFlags, size_t dataSize) -> bng_expected<StagingBufferInfo> {
	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = dataSize,
		.usage = usageFlags | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VmaAllocationCreateInfo vmaAllocateInfo{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
		.requiredFlags = 0,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = VK_NULL_HANDLE,
		.pUserData = nullptr,
		.priority = 0.0f
	};
	VkBuffer buffer;
	VmaAllocation allocation;
	auto vkResult = vmaCreateBuffer(allocator, &bufferCreateInfo, &vmaAllocateInfo, &buffer, &allocation, nullptr);
	if (vkResult != VK_SUCCESS) {
		return bng_unexpected<StagingBufferInfo>("vmaCreateBuffer failed");
	}

	// get the mapped memory location
	VmaAllocationInfo allocationInfo;
	vmaGetAllocationInfo(allocator, allocation, &allocationInfo);
	void* data = allocationInfo.pMappedData;
	return StagingBufferInfo{
		.sizeInBytes = dataSize,
		.mappedData = data,
		.buffer = buffer,
		.vmaInfo = allocation
	};
}

auto deleteStagingBuffer(VmaAllocator allocator, const StagingBufferInfo& buffer) -> bng_expected<void> {
	vmaDestroyBuffer(allocator, buffer.buffer, buffer.vmaInfo);

	return {};
}

export
template <VkBufferUsageFlags BufferType>
class StagingBufferPool {
public:
	StagingBufferPool(VmaAllocator allocator, size_t maxBuffers = 4, size_t startSize = 1024) : vma_allocator_(allocator), store_count_semaphore_(maxBuffers) {
		for (int ix = 0; ix < maxBuffers; ix++) {
			createStagingBuffer(vma_allocator_, BufferType, startSize)
				.transform([this](StagingBufferInfo s) { available_buffers_.push_back(s); });
		}
	}
	~StagingBufferPool() {
		for (auto s : available_buffers_) {
			deleteStagingBuffer(vma_allocator_, s);
		}
	}

	auto acquireStagingBufferTask(size_t requestedSize) -> coro::task<bng_expected<StagingBufferInfo>> {
		// first wait on the semaphore - once we pass this we know at least one buffer is available
		auto bufferAvailable = co_await store_count_semaphore_.acquire();
		if (bufferAvailable != coro::semaphore::acquire_result::acquired) {
			co_return bainangua::bng_unexpected<StagingBufferInfo>("failed to acquire staging buffer semaphore");
		}
		
		auto storeLock = co_await store_mutex_.lock();
		// if we passed the semaphore above, there should be at least one staging buffer avialable
		if (available_buffers_.size() == 0) {
			co_return bng_unexpected<StagingBufferInfo>("no available staging buffers found");
		}
		// look for a buffer large enough for the request
		auto suitableBuffer = std::ranges::find_if(available_buffers_, [=](StagingBufferInfo& s) {return s.sizeInBytes >= requestedSize; });
		if (suitableBuffer != available_buffers_.end()) {
			// nothing suitable was found, we'll replace one of the current buffers with a new buffer that IS big enough
			StagingBufferInfo s = available_buffers_.back();
			available_buffers_.pop_back();
			deleteStagingBuffer(vma_allocator_, s);
			co_return createStagingBuffer(vma_allocator_, BufferType, requestedSize);
		}
		else {
			StagingBufferInfo buffer = *suitableBuffer;
			available_buffers_.erase(suitableBuffer);
			co_return buffer;
		}
	}

	auto releaseStagingBufferTask(StagingBufferInfo b) -> coro::task<void> {
		// move the buffer back into the available pool
		coro::scoped_lock storeLock = co_await store_mutex_.lock();
		available_buffers_.push_back(b);
		store_count_semaphore_.release();
	}

private:
	coro::semaphore store_count_semaphore_;
	coro::mutex store_mutex_;
	VmaAllocator vma_allocator_;
	std::vector<StagingBufferInfo> available_buffers_;
};


export
template <VkBufferUsageFlags BufferType>
using StagingBufferPoolKey = bainangua::SingleResourceKey<void, std::shared_ptr<StagingBufferPool<BufferType>>>;

export
template <VkBufferUsageFlags BufferType>
auto stagingBufferPoolLoader = boost::hana::make_pair(
	boost::hana::type_c<StagingBufferPoolKey<BufferType>>,
	[]<typename Resources, typename Storage>(bainangua::ResourceLoader<Resources, Storage>&loader, StagingBufferPoolKey<BufferType> poolkey) -> bainangua::LoaderRoutine<std::shared_ptr<StagingBufferPool<BufferType>>> {
		
		std::shared_ptr<StagingBufferPool<BufferType>> newPool(std::make_shared<StagingBufferPool<BufferType>>(loader.context_.vmaAllocator));

		co_return bainangua::bng_expected<bainangua::LoaderResults<std::shared_ptr<StagingBufferPool<BufferType>>>>(
			{
				.resource_ = newPool,
				.unloader_ = [](std::shared_ptr<StagingBufferPool<BufferType>> pool) -> coro::task<bainangua::bng_expected<void>> {
					// should be auto-deleted once the shared_ptr<> refs vanish
					co_return{};
				}(newPool)
			}
		);
	}
);

export
template <VkBufferUsageFlags BufferType>
constexpr auto acquireStagingBuffer =
	[]<typename Resources, typename Storage>(std::shared_ptr<bainangua::ResourceLoader<Resources, Storage>> loader, size_t requestSize) -> coro::task<bng_expected<StagingBufferInfo>> {
	// first need to acquire/load a staging buffer pool
	bng_expected<std::shared_ptr<StagingBufferPool<BufferType>>> poolResult = co_await loader->loadResource(StagingBufferPoolKey<BufferType>());
	if (!poolResult) {
		co_return bainangua::bng_unexpected<StagingBufferInfo>(poolResult.error());
	}

	auto buffer = co_await poolResult.value()->acquireStagingBufferTask(requestSize);
	if (buffer) {

		co_return bainangua::bng_expected<StagingBufferInfo>(buffer.value());
	}
	else {
		co_return bainangua::bng_unexpected<StagingBufferInfo>(buffer.error());
	}
};


export
template <VkBufferUsageFlags BufferType, typename Resources, typename Storage>
auto releaseStagingBuffer(std::shared_ptr<bainangua::ResourceLoader<Resources, Storage>> loader, StagingBufferInfo buffer) -> coro::task<bng_expected<void>> {
	// first need to acquire/load a staging buffer pool
	bng_expected<std::shared_ptr<StagingBufferPool<BufferType>>> poolResult = co_await loader->loadResource(StagingBufferPoolKey<BufferType>());
	if (!poolResult) {
		co_return bng_unexpected<void>(poolResult.error());
	}

	co_await poolResult.value()->releaseStagingBufferTask(buffer);
	co_return {};
}

export struct Argh {};

export
template <VkBufferUsageFlags BufferType>
struct CreateStagingBufferAsResource {
	CreateStagingBufferAsResource(size_t s) : requestSize(s) {}

	size_t requestSize;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr bng_expected<bool> wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		auto       loader = boost::hana::at_key(r, BOOST_HANA_STRING("resourceLoader"));

		bng_expected<StagingBufferInfo> buffer = coro::sync_wait(acquireStagingBuffer<BufferType>(loader, requestSize));
		if (!buffer.has_value()) {
			return bng_unexpected<bool>(buffer.error());
		}

		auto rWithBuffer = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("stagingBuffer"), buffer.value()));
		auto applyResult = f.applyRow(rWithBuffer);

		coro::sync_wait(releaseStagingBuffer<BufferType>(loader, buffer.value()));
		return true;
	}
};

}