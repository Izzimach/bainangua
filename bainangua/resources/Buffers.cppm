/**
* Type-erased interface for buffers, including: Vertex Buffers, Index Buffers, Uniform Buffers.
* 
*/

module;

#include "bainangua.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <variant>
#include <vector>
#include <coro/coro.hpp>
#include <tanuki.hpp>


export module Buffers;

import VulkanContext;
import CommandQueue;

namespace bainangua {

//
// type-erased static buffer interface
//

template <typename Base, typename Holder, typename T>
struct static_buffer_iface_impl : public Base {
    auto release() -> void override
    {
        getval<Holder>(this).release();
    }
};

export
struct static_buffer_iface {
    virtual auto release() -> void = 0;

    template <typename Base, typename Holder, typename T>
    using impl = static_buffer_iface_impl<Base, Holder, T>;
};

struct static_buffer_ref_iface {
    template <typename Wrap>
    struct impl {
        TANUKI_REF_IFACE_MEMFUN(release)
    };
};

export
using bng_static_buffer = tanuki::wrap<static_buffer_iface, tanuki::config<void, static_buffer_ref_iface>{.pointer_interface = false } > ;



export
struct generic_buffer {
    vk::Buffer buffer_handle_;
    VmaAllocation allocation_;
    VmaAllocator allocator_;

    auto release() -> void
    {
        vmaDestroyBuffer(allocator_, buffer_handle_, allocation_);
        buffer_handle_ = VK_NULL_HANDLE;
    }
};

[[nodiscard]] auto putDataIntoHostBuffer(generic_buffer buffer, void* data, std::size_t dataSize) -> bng_expected<void>
{
    // dump data into buffer
    void* bufferMemory;
    auto mapResult = vmaMapMemory(buffer.allocator_, buffer.allocation_, &bufferMemory);
    if (mapResult != VK_SUCCESS) {
        return bng_unexpected("vmaMapMemory failed");
    }
    memcpy(bufferMemory, data, dataSize);
    vmaUnmapMemory(buffer.allocator_, buffer.allocation_);

    return {};
}

export auto allocateStaticHostBuffer(VmaAllocator allocator, VkBufferUsageFlags usage, void* data, std::size_t dataSize) -> bng_expected<generic_buffer>
{
    VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = dataSize,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo vmaAllocateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
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

    return putDataIntoHostBuffer(generic_buffer{ buffer,allocation, allocator }, data, dataSize)
        .transform([=]() {
            return generic_buffer{ buffer, allocation, allocator };
        })
        .map_error([=](auto error) {
            vmaDestroyBuffer(allocator, buffer, allocation);
            return error;
        });
}

export
[[nodiscard]] auto allocateStagingBuffer(VmaAllocator allocator, VkBufferUsageFlags usage, std::size_t dataSize) -> bng_expected<generic_buffer>
{
    VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = dataSize,
        .usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo vmaAllocateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
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
    return generic_buffer{ buffer,allocation, allocator };
}

export
[[nodiscard]] auto allocateStaticGPUBuffer(VmaAllocator allocator, VkBufferUsageFlags usage, void* data, std::size_t dataSize, generic_buffer stagingBuffer, vk::CommandBuffer cmd, std::shared_ptr<CommandQueueFunnel> queue, coro::thread_pool &threads) -> coro::task<bng_expected<generic_buffer>>
{
    VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = dataSize,
        .usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VmaAllocationCreateInfo vmaAllocateInfo{
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
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
        co_return bng_unexpected("vmaCreateBuffer failed");
    }

    // We prefer GPU local memory, but VMA may have allocated in host memory for whatever reason
    // If this is the case, just dump straight into the buffer
    VkMemoryPropertyFlags flags;
    vmaGetAllocationMemoryProperties(allocator, allocation, &flags);
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        co_return putDataIntoHostBuffer(generic_buffer{ buffer,allocation, allocator }, data, dataSize)
            .transform([=]() {
                return generic_buffer{ buffer, allocation, allocator };
            })
            .map_error([=](auto error) {
                vmaDestroyBuffer(allocator, buffer, allocation);
                return error;
            });
    }

    // it's not host visible, so transfer using a staging buffer and a transfer command
    auto stagingResult = putDataIntoHostBuffer(stagingBuffer, data, dataSize);
    if (!stagingResult) { // staging error
        vmaDestroyBuffer(allocator, buffer, allocation);
        co_return bng_unexpected("allocateStaticGPUBuffer: error writing to staging buffer: " + stagingResult.error());
    }

    // copy using the command buffer
    vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    vk::Result commandBeginResult = cmd.begin(&beginInfo);
    if (commandBeginResult != vk::Result::eSuccess) {
        vmaDestroyBuffer(allocator, buffer, allocation);
        co_return bng_unexpected("allocateStaticGPUBuffer: failed to start command buffer");
    }
    vk::BufferCopy copyRegion(0, 0, dataSize);
    cmd.copyBuffer(stagingBuffer.buffer_handle_, buffer, 1, &copyRegion);
    cmd.end();

    vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &cmd, 0, nullptr);
    co_await queue->awaitCommand(submitInfo, threads);

    co_return generic_buffer{ buffer, allocation, allocator };
}

}
