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
struct LoaderResults {
    ResourceType resource_;
    std::optional<coro::task<bng_expected<void>>> unloader_;
};

export
template <typename ResourceType>
using LoaderRoutine = coro::task<bainangua::bng_expected<LoaderResults<ResourceType>>>;

export
template <typename ResourceType>
struct SingleResourceStore {
    SingleResourceStore(size_t refCount = 0) 
    : resourceValue_(tl::make_unexpected("Resource not yet loaded")),
      refCount_(refCount)

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
    std::optional<coro::task<bng_expected<void>>> unloader_;

    // this gets signalled once the resource is ready.
    coro::event loadedEvent_;

    // some coroutines need to modify multiple fields at once, so we can't get away with making some fields
    // atomic<>. Instead use this mutex when modifying fields
    coro::mutex resourceMutex_;

    // The resource treated as a value.  You may need to used a shared_ptr here if you don't want
    // the resource itself getting copied everywhere.
    bng_expected<ResourceType> resourceValue_;

    size_t refCount_;
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
        loaders_(loaders),
        tp(std::make_shared<coro::thread_pool>(
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
        )),
        autoTasks(tp)
    {}

    ResourceLoader(VulkanContext& context, LoaderDirectory loaders)
        : vkInstance_(context.vkInstance),
        vkDevice_(context.vkDevice),
        vkPhysicalDevice_(context.vkPhysicalDevice),
        vmaAllocator_(context.vmaAllocator),
        loaders_(loaders),
        tp(std::make_shared<coro::thread_pool>(
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
        )),
        autoTasks(tp)
    {}

    ResourceLoader(ResourceLoader const&) = delete;
    ResourceLoader(ResourceLoader&) = delete;
    ResourceLoader(ResourceLoader&&) = delete;

    void operator=(ResourceLoader&) = delete;
    void operator=(ResourceLoader const &) = delete;
    void operator=(ResourceLoader&&) = delete;

    ~ResourceLoader() {
        // w
        coro::sync_wait(autoTasks.garbage_collect_and_yield_until_empty());
        tp->shutdown();
        std::cout << "ResourceLoader destructor\n";
    }

    vk::Instance vkInstance_;
    vk::Device   vkDevice_;
    vk::PhysicalDevice vkPhysicalDevice_;
    VmaAllocator vmaAllocator_;
    LoaderDirectory loaders_;
    LoaderStorage storage_;
    
    // to avoid deadlock always lock the storage_mutex_ and THEN lock the resourceMutex_ of the
    // relevant resource.
    coro::mutex storage_mutex_;


    template <typename LookupKey>
    auto loadResource(LookupKey key) -> coro::task<bng_expected<typename LookupKey::resource_type>> {
        return [](ResourceLoader* self, LookupKey key) -> coro::task<bng_expected<typename LookupKey::resource_type>> {
            auto& specificStorage = boost::hana::at_key(self->storage_, boost::hana::type_c<LookupKey>);
            coro::scoped_lock storageLock = co_await self->storage_mutex_.lock();
            auto resourceIter = specificStorage.find(key);

            if (resourceIter != specificStorage.end()) {
                SingleResourceStore<LookupKey::resource_type>* resourceStore = resourceIter->second.get();
                {
                    coro::scoped_lock resourceLock = co_await resourceStore->resourceMutex_.lock();
                    resourceStore->refCount_++;
                }
                storageLock.unlock();

                co_return co_await resourceStore->getterTask();
            }
            else {
                // first insert a ResourceStore to mark where the resource will be put, so that other threads/tasks can wait on it
                // note we are still under the storage lock mutex here...
                std::unique_ptr<SingleResourceStore<LookupKey::resource_type>> storePtr(new SingleResourceStore<LookupKey::resource_type>(1));
                storePtr->unloader_ = std::nullopt;
                auto emplaceResult = boost::hana::at_key(self->storage_, boost::hana::type_c<LookupKey>).emplace(key, std::move(storePtr));

                std::shared_ptr<SingleResourceStore<LookupKey::resource_type>> resourceStore = std::get<0>(emplaceResult)->second;
                auto getter = resourceStore->getterTask();

                storageLock.unlock();

                self->enqueueLoader(key, resourceStore);

                co_return co_await getter;
            }
        }(this, key);
    }

    template <typename LookupKey>
    void enqueueLoader(LookupKey key, std::shared_ptr<SingleResourceStore<typename LookupKey::resource_type>> storePtr)
    {
        // this is a coroutine, no lambda capture for me
        auto loadAndGo = [](ResourceLoader* self, LookupKey key, std::shared_ptr<SingleResourceStore<LookupKey::resource_type>> storePtr) -> coro::task<void> {
            auto& loader = boost::hana::at_key(self->loaders_, boost::hana::type_c<LookupKey>);
            bng_expected<LoaderResults<LookupKey::resource_type>> result = co_await loader(*self, key);

            coro::scoped_lock resourceLock = co_await storePtr->resourceMutex_.lock();
            if (result.has_value()) {
                storePtr->resourceValue_ = result.value().resource_;
                storePtr->unloader_ = std::move(result.value().unloader_);
            }
            
            else {
                storePtr->resourceValue_ = bng_unexpected<LookupKey::resource_type>(result.error());
            }

            // signal the event to wake up waiters. We do this even if the loader failed, so that waiters
            // will see the error
            storePtr->loadedEvent_.set();
            co_return;
        };
        autoTasks.start(loadAndGo(this, key, storePtr));
    }

    template <typename LookupKey>
    auto unloadResource(LookupKey key) -> coro::task<void> {
        return [](ResourceLoader* self, LookupKey key) -> coro::task<void> {
            auto& specificStorage = boost::hana::at_key(self->storage_, boost::hana::type_c<LookupKey>);

            coro::scoped_lock storageLock = co_await self->storage_mutex_.lock();
            auto resourceiter = specificStorage.find(key);
            if (resourceiter == specificStorage.end()) {
                throw std::exception("Attempt to unload resource, but it is not loaded");
            }
            else
            {
                std::shared_ptr<SingleResourceStore<LookupKey::resource_type>> resourceStore = resourceiter->second;
                coro::scoped_lock resourceLock = co_await resourceStore->resourceMutex_.lock();

                size_t refCount = --(resourceStore->refCount_);
                if (refCount == 0) {
                    std::cout << "unloading\n";
                    // modify the resource so that it appears unloaded and start the unload process

                    // Note that this reset() will spinlock until all event waiters have been handled/cleared.
                    // However there shouldn't be any waiters since the refCount is zero
                    resourceStore->loadedEvent_.reset();

                    resourceStore->resourceValue_ = tl::make_unexpected(std::string("resource not loaded"));

                    if (resourceStore->unloader_.has_value()) {
                        coro::task<bng_expected<void>> unloaderTask = std::move(resourceStore->unloader_.value());
                        resourceStore->unloader_ = std::nullopt;

                        self->autoTasks.start([](ResourceLoader* self, LookupKey key, std::shared_ptr<SingleResourceStore<LookupKey::resource_type>> resourceStore, auto t) -> coro::task<void> {
                            auto result = co_await t;

                            // the resource is fully unloaded.
                            // did someone try to load this resource while we were unloading it? if so, we need to
                            // reload it :(
                            coro::scoped_lock storageLock = co_await self->storage_mutex_.lock();
                            coro::scoped_lock resourceLock = co_await resourceStore->resourceMutex_.lock();
                            if (resourceStore->refCount_ == 0) {
                                // remove this entry
                                auto& specificStorage = boost::hana::at_key(self->storage_, boost::hana::type_c<LookupKey>);
                                specificStorage.erase(key);
                            }
                            else // someone tried to load it while we were unloading and they are now waiting on the event
                            {
                                self->enqueueLoader(key, resourceStore);
                            }

                            co_return;
                            } (self, key, resourceStore, std::move(unloaderTask))
                        );
                    }
                    else
                    {
                        // we don't have to wait for an unloader... just remove the storage
                        specificStorage.erase(key);
                    }
                }
            }
        }(this, key);
    }

private:
    std::shared_ptr<coro::thread_pool> tp;

    coro::task_container<coro::thread_pool> autoTasks;
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
