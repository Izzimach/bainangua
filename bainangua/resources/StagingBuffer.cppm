/**
* Module for handling staging buffers.
* To get buffer data to GPU-side memory we normally dump it into a CPU-side staging buffer and then use a transfer command to copy the data into a
* GPU-side buffer.
* 
* We don't want to allocate/deallocate a staging buffer every time data needs to be transferred; instead we cache a pool of
* a few staging buffers. When someone needs a staging buffer they specify the size and then wait until a buffer of adequate size
* becomes available.
* 
* If no one is using the pool it is deallocated, but typically if you are loading one texture or set of vertex data you are loading several, so this should (hopefully)
* help prevent constant loading/unloading of the buffer pool.
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

import VulkanContext;

namespace bainangua {

template <VkBufferUsageFlags BufferType> class StagingBufferPool;

export
template <VkBufferUsageFlags BufferType>
struct StagingBuffer {
	size_t sizeInBytes;
	void* mappedData;
	vk::Buffer buffer;
	VmaAllocation vmaInfo;
	StagingBufferPool<BufferType> *sourcePool;
};

template <VkBufferUsageFlags BufferType>
auto createStagingBuffer(VmaAllocator allocator, VkBufferUsageFlags usageFlags, size_t dataSize, StagingBufferPool<BufferType> *pool) -> bng_expected<StagingBuffer<BufferType>> {
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
		return bng_unexpected("vmaCreateBuffer failed");
	}

	// get the mapped memory location
	VmaAllocationInfo allocationInfo;
	vmaGetAllocationInfo(allocator, allocation, &allocationInfo);
	void* data = allocationInfo.pMappedData;
	return StagingBuffer{
		.sizeInBytes = dataSize,
		.mappedData = data,
		.buffer = buffer,
		.vmaInfo = allocation,
		.sourcePool = pool
	};
}

template <VkBufferUsageFlags BufferType>
auto deleteStagingBuffer(VmaAllocator allocator, const StagingBuffer<BufferType>& buffer) -> bng_expected<void> {
	vmaDestroyBuffer(allocator, buffer.buffer, buffer.vmaInfo);

	return {};
}

export
template <VkBufferUsageFlags BufferType>
class StagingBufferPool {
public:
	StagingBufferPool(VmaAllocator allocator, size_t maxBuffers = 4, size_t startSize = 1024) : vma_allocator_(allocator), store_count_semaphore_(maxBuffers) {
		for (int ix = 0; ix < maxBuffers; ix++) {
			createStagingBuffer(vma_allocator_, BufferType, startSize, this)
				.transform([this](StagingBuffer<BufferType> s) { available_buffers_.push_back(s); });
		}
	}
	~StagingBufferPool() {
		for (auto s : available_buffers_) {
			deleteStagingBuffer(vma_allocator_, s);
		}
	}

	auto acquireStagingBufferTask(size_t requestedSize) -> coro::task<bng_expected<StagingBuffer<BufferType>>> {
		// first wait on the semaphore - once we pass this we know at least one buffer is available
		auto bufferAvailable = co_await store_count_semaphore_.acquire();
		if (bufferAvailable != coro::semaphore::acquire_result::acquired) {
			co_return bainangua::bng_unexpected("failed to acquire staging buffer semaphore");
		}
		
		auto storeLock = co_await store_mutex_.lock();
		// if we passed the semaphore above, there should be at least one staging buffer avialable
		if (available_buffers_.size() == 0) {
			co_return bng_unexpected("no available staging buffers found");
		}
		// look for a buffer large enough for the request
		auto suitableBuffer = std::ranges::find_if(available_buffers_, [=](StagingBuffer<BufferType>& s) {return s.sizeInBytes >= requestedSize; });
		if (suitableBuffer != available_buffers_.end()) {
			// nothing suitable was found, we'll replace one of the current buffers with a new buffer that IS big enough
			StagingBuffer<BufferType> s = available_buffers_.back();
			available_buffers_.pop_back();
			deleteStagingBuffer(vma_allocator_, s);
			co_return createStagingBuffer(vma_allocator_, BufferType, requestedSize, this);
		}
		else {
			StagingBuffer<BufferType> buffer = *suitableBuffer;
			available_buffers_.erase(suitableBuffer);
			co_return buffer;
		}
	}

	auto releaseStagingBufferTask(StagingBuffer<BufferType> b) -> coro::task<void> {
		// move the buffer back into the available pool
		coro::scoped_lock storeLock = co_await store_mutex_.lock();
		available_buffers_.push_back(b);
		store_count_semaphore_.release();
	}

private:
	coro::semaphore store_count_semaphore_;
	coro::mutex store_mutex_;
	VmaAllocator vma_allocator_;
	std::vector<StagingBuffer<BufferType>> available_buffers_;
};

export
template <VkBufferUsageFlags BufferType>
struct CreateStagingBuffer {
	CreateStagingBuffer(size_t s) : requestSize(s) {}

	size_t requestSize;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr bng_expected<bool> wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		VmaAllocator vmaAllocator = boost::hana::at_key(r, BOOST_HANA_STRING("vmaAllocator"));

		StagingBufferPool pool(vmaAllocator);
		bng_expected<StagingBuffer<BufferType>> buffer = coro::sync_wait(pool.acquireStagingBufferTask(requestSize));
		if (!buffer.has_value()) {
			return bng_unexpected(buffer.error());
		}

		auto rWithBuffer = boost::hana::insert(r, boost::hana::make_pair(BOOST_HANA_STRING("stagingBuffer"), buffer.value()));
		auto applyResult = f.applyRow(rWithBuffer);

		coro::sync_wait(pool.releaseStagingBufferTask<BufferType>(buffer.value()));
		return true;
	}
};

}