module;

#include "bainangua.hpp"
#include "RowType.hpp"

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/string.hpp>
#include <coro/coro.hpp>
#include <mutex>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

#include <gtl/phmap.hpp>

export module ResourceLoader;

import VulkanContext;

namespace bainangua {

export struct SingleResourceTag {};

export
template <typename LookupType, typename ResourceType>
struct SingleResourceKey {
    using hana_tag = bainangua::SingleResourceTag;

    using resource_type = ResourceType;

    LookupType key;
};

export
template <typename LookupType, typename ResourceType>
constexpr bool operator==(const SingleResourceKey<LookupType, ResourceType>& a, const SingleResourceKey<LookupType, ResourceType>& b) {
    return a.key == b.key;
}


export
template <typename ResourceType>
struct SingleResourceStore {
    SingleResourceStore() 
    : resourceValue_(tl::make_unexpected("Resource not yet loaded")),
      refCount_(0)

    {}

    // co_await on the returned task to get the resource (or an error). The co_await will
    // return immediately if the resource is already available, or it will wait if the
    // resource needs loading/is still loading
    coro::task<bng_expected<ResourceType>> getterTask() {
        return [](SingleResourceStore *self) -> coro::task<bng_expected<ResourceType>> {
            co_await self->loadedEvent_;
            co_return self->resourceValue_;
        }(this);
    }

    // co_await on this to clean up whatever resources were reserved/allocated.
    std::optional<coro::task<bng_expected<void>>> unloader;

    // this gets signalled once the resource is ready.
    coro::event loadedEvent_;

    // The resource treated as a value.  You may need to used a shared_ptr here if you don't want
    // the resource itself getting copied everywhere.
    bng_expected<ResourceType> resourceValue_;

    std::atomic<size_t> refCount_;
};


export
template <typename LoaderDirectory, typename LoaderStorage>
class ResourceLoader
{
public:
    template <typename Row>
    ResourceLoader(Row r, LoaderDirectory loaders)
        : vkInstance_(boost::hana::at_key(r, BOOST_HANA_STRING("instance"))),
        vkDevice_(boost::hana::at_key(r, BOOST_HANA_STRING("device"))),
        vkPhysicalDevice_(boost::hana::at_key(r, BOOST_HANA_STRING("physicalDevice"))),
        vmaAllocator_(boost::hana::at_key(r, BOOST_HANA_STRING("vmaAllocator"))),
        loaders_(loaders)
    {}

    ResourceLoader(VulkanContext& context, LoaderDirectory loaders)
        : vkInstance_(context.vkInstance),
        vkDevice_(context.vkDevice),
        vkPhysicalDevice_(context.vkPhysicalDevice),
        vmaAllocator_(context.vmaAllocator),
        loaders_(loaders)
    {}

    ResourceLoader(ResourceLoader const&) = delete;
    ResourceLoader(ResourceLoader&) = delete;
    ResourceLoader(ResourceLoader&&) = delete;

    void operator=(ResourceLoader&) = delete;
    void operator=(ResourceLoader const &) = delete;
    void operator=(ResourceLoader&&) = delete;

    ~ResourceLoader() {
        std::cout << "ResourceLoader destructor\n";
    }

    vk::Instance vkInstance_;
    vk::Device   vkDevice_;
    vk::PhysicalDevice vkPhysicalDevice_;
    VmaAllocator vmaAllocator_;
    LoaderDirectory loaders_;
    LoaderStorage storage_;
    std::mutex storage_mutex_;


    template <typename LookupKey>
    constexpr auto loadResource(LookupKey key) -> coro::task<bng_expected<typename LookupKey::resource_type>> {
        std::lock_guard lock(this->storage_mutex_);
        auto &specificStorage = boost::hana::at_key(this->storage_, boost::hana::type_c<LookupKey>);
        auto resourceiter = specificStorage.find(key);
        if (resourceiter != specificStorage.end()) {
            std::cout << "Already loading resource, waiting...\n";
            SingleResourceStore<LookupKey::resource_type>* resourceStore = resourceiter->second.get();
            return resourceStore->getterTask();
        }
        else {
            // first insert a ResourceStore to marked where the resource will be put, so that other threads/tasks can wait on it
            // note we are still under the lock_guard here...
            std::unique_ptr<SingleResourceStore<LookupKey::resource_type>> storePtr(new SingleResourceStore<LookupKey::resource_type>());
            boost::hana::at_key(this->storage_, boost::hana::type_c<LookupKey>).emplace(key,std::move(storePtr));

            // this is a coroutine, no lambda capture for me
            auto loadAndGo = [](ResourceLoader* self, LookupKey key) -> coro::task<bng_expected<typename LookupKey::resource_type>> {
                auto &loader = boost::hana::at_key(self->loaders_, boost::hana::type_c<LookupKey>);
                auto loadResult = co_await loader(self->tp, *self, key);
                if (loadResult.has_value()) {
                    std::lock_guard lock(self->storage_mutex_);
                    auto &specificStorage = boost::hana::at_key(self->storage_, boost::hana::type_c<LookupKey>);
                    auto resourceiter = specificStorage.find(key);
                    if (resourceiter != specificStorage.end()) {
                        SingleResourceStore<LookupKey::resource_type>* storePtr = resourceiter->second.get();
                        // signal the event to wake up waiters
                        storePtr->resourceValue_ = loadResult.value();
                        storePtr->loadedEvent_.set();
                        co_return storePtr->resourceValue_;
                    }
                    else // ERROR
                    {
                        co_return tl::make_unexpected("Could not find resource");
                    }
                }
                else {
                    co_return loadResult;
                }

                };
            return loadAndGo(this, key);
        }
    }

    template <typename LookupKey>
    constexpr auto unloadResource(LookupKey key) -> void {

    }

private:
    coro::thread_pool tp{
        coro::thread_pool::options{
            // By default all thread pools will create its thread count with the
            // std::thread::hardware_concurrency() as the number of worker threads in the pool,
            // but this can be changed via this thread_count option.  This example will use 4.
            .thread_count = 4,
            // Upon starting each worker thread an optional lambda callback with the worker's
            // index can be called to make thread changes, perhaps priority or change the thread's
            // name.
            .on_thread_start_functor = [](std::size_t worker_idx) -> void {
                std::cout << "thread pool worker " << worker_idx << " is starting up.\n";
            },
            // Upon stopping each worker thread an optional lambda callback with the worker's
            // index can b called.
            .on_thread_stop_functor = [](std::size_t worker_idx) -> void {
                std::cout << "thread pool worker " << worker_idx << " is shutting down.\n";
            }
        }
    };

};

}


export
template <>
struct boost::hana::hash_impl<bainangua::SingleResourceTag> {
    template <typename SingleResourceStore>
    static constexpr auto apply(SingleResourceStore const&) {
        return hana::type_c<SingleResourceStore>;
    }
};


namespace std {

    export
    template <typename LookupType, typename ResourceType>
    struct hash<bainangua::SingleResourceKey<LookupType, ResourceType>> {
        constexpr size_t operator()(bainangua::SingleResourceKey<LookupType, ResourceType> s) {
            hash<decltype(s.key)> hasher;
            return hasher(s.key);
        }
        constexpr size_t operator()(const bainangua::SingleResourceKey<LookupType, ResourceType>& s) const {
            hash<decltype(s.key)> hasher;
            return hasher(s.key);
        }
        constexpr size_t operator()(const bainangua::SingleResourceKey<LookupType, ResourceType>& s) {
            hash<decltype(s.key)> hasher;
            return hasher(s.key);
        }
    };

}
