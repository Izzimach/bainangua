//
// This vulkan context handles most of the initial outer layers involving initialization and tear down, including:
//  - getting extensions
//  - creating a vkInstance
//  - creating a window and physical device
//  - creating a surface
//
//  This part specifically does NOT create a swapchain or handle any sort of memory allocation.
//
module;

#include "bainangua.hpp"
#include "RowType.hpp"

#include <array>
#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <concepts>
#include <coroutine>
#include <chrono>
#include <coroutine>
#include <expected.hpp> // note: tl::expected for c++<23
#include <fmt/format.h>
#include <functional>
#include <memory_resource>
#include <ranges>
#include <string_view>
#include <thread>
#include <vector>


export module VulkanContext;


namespace bainangua {
    
/*!
 * A Vulkan context. This includes all the basic Vulkan data created upon initialization, such as
 * a vk::Instance or vk::Device. This also includes a coroutine to call at the end of each frame.
 */
export struct VulkanContext {
    vk::Instance vkInstance;   //!< The vulkan instance. Don't destroy this yourself!
    GLFWwindow* glfwWindow;
    vk::PhysicalDevice vkPhysicalDevice;
    vk::Device vkDevice;
    vk::SurfaceKHR vkSurface;

    uint32_t graphicsQueueFamilyIndex;
    vk::Queue graphicsQueue;
    uint32_t presentQueueFamilyIndex;
    vk::Queue presentQueue;

    std::coroutine_handle<> endOfFrame; //!< your VulkanContextConfig::innerCode function should call this at 'end-of-frame'

    VmaAllocator vmaAllocator; //!< for graphics memory allocation via vma

    bool windowResized; //!< flag that the window was resized
};


export struct VulkanContextConfig {
    std::string AppName{ "Vulkan App" };
    std::string EngineName{ "Default Vulkan Engine" };
    std::vector<std::string> requiredExtensions;

    bool useValidation;

    std::function<bool(VulkanContext&)> innerCode; //!< called once all the Vulkan handles are created for the VulkanContext
};


struct ReturnObject {
    struct promise_type {
        ReturnObject get_return_object() {
            return {
                // Uses C++20 designated initializer syntax
                .h_ = std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
        void return_void() {}
    };

    std::coroutine_handle<promise_type> h_;
    operator std::coroutine_handle<promise_type>() const { return h_; }
    // A coroutine_handle<promise_type> converts to coroutine_handle<>
    operator std::coroutine_handle<>() const { return h_; }
};

ReturnObject doNothingEndFrame()
{
    for (unsigned i = 0;; ++i) {
        co_await std::suspend_always();
        fmt::print("counter: {}\n", i);
    }
}


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT /*messageSeverity*/,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) {

    fmt::print("Validation message: [{}]\n", pCallbackData->pMessage);

    return VK_FALSE;
}

static void framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto s = reinterpret_cast<bainangua::VulkanContext*>(glfwGetWindowUserPointer(window));
    if (s) s->windowResized = true;
}


template <typename Row>
tl::expected<int, std::string> invokeInnerCode(Row r)
{
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("config"), VulkanContextConfig>, "Row must have field named 'config'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("instance"), vk::Instance>, "Row must have field named 'instance'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("glfwWindow"), GLFWwindow*>, "Row must have field named 'glfwWindow'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("physicalDevice"), vk::PhysicalDevice>, "Row must have field named 'physicalDevice'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("device"), vk::Device>, "Row must have field named 'device'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("surface"), vk::SurfaceKHR>, "Row must have field named 'surface'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("graphicsQueueFamilyIndex"), uint32_t>, "Row must have field named 'graphicsQueueFamilyIndex'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("graphicsQueue"), vk::Queue>, "Row must have field named 'graphicsQueue'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("presentQueueFamilyIndex"), uint32_t>, "Row must have field named 'presentQueueFamilyIndex'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("presentQueue"), vk::Queue>, "Row must have field named 'presentQueue'");
    static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("vmaAllocator"), VmaAllocator>, "Row must have field named 'vmaAllocator'");

    const VulkanContextConfig config  = boost::hana::at_key(r, BOOST_HANA_STRING("config"));
    vk::Instance instance             = boost::hana::at_key(r, BOOST_HANA_STRING("instance"));
    GLFWwindow* window                = boost::hana::at_key(r, BOOST_HANA_STRING("glfwWindow"));
    vk::PhysicalDevice physicalDevice = boost::hana::at_key(r, BOOST_HANA_STRING("physicalDevice"));
    vk::Device device                 = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
    vk::SurfaceKHR surface            = boost::hana::at_key(r, BOOST_HANA_STRING("surface"));
    uint32_t graphicsQueueFamilyIndex = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsQueueFamilyIndex"));
    vk::Queue graphicsQueue           = boost::hana::at_key(r, BOOST_HANA_STRING("graphicsQueue"));
    uint32_t presentQueueFamilyIndex  = boost::hana::at_key(r, BOOST_HANA_STRING("presentQueueFamilyIndex"));
    vk::Queue presentQueue            = boost::hana::at_key(r, BOOST_HANA_STRING("presentQueue"));
    VmaAllocator graphicsAllocator    = boost::hana::at_key(r, BOOST_HANA_STRING("vmaAllocator"));

    try
    {
        VulkanContext vkState{ instance, window, physicalDevice, device, surface, graphicsQueueFamilyIndex, graphicsQueue, presentQueueFamilyIndex, presentQueue, doNothingEndFrame(), graphicsAllocator, false };
        glfwSetWindowUserPointer(window, &vkState);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

        bool result = config.innerCode(vkState);
        if (!result) {
            fmt::print("inner code returned false\n");
        }
        glfwSetWindowUserPointer(window, nullptr);
    }
    catch (vk::SystemError& err)
    {
        fmt::print("vk::SystemError: {}\n", err.what());
        return tl::make_unexpected(err.what());
    }
    catch (std::exception& err)
    {
        fmt::print("std::exception: {}\n", err.what());
        return tl::make_unexpected(err.what());
    }
    catch (...)
    {
        fmt::print("unknown error!\n");
        return tl::make_unexpected("unknown error!");
    }

    return 0;
}

struct InvokeInnerCode {
    using row_tag = RowType::RowFunctionTag;
    using return_type = tl::expected<int,std::string>;

    template<typename Row>
    static constexpr tl::expected<int,std::string> applyRow(Row r) { return invokeInnerCode(r); }
};

struct StandardVMAAllocator {
    using row_tag = RowType::RowWrapperTag;

    template <typename WrappedReturnType>
    using return_type_transformer = WrappedReturnType;

    template <typename RowFunction, typename Row>
    static constexpr RowFunction::return_type wrapRowFunction(RowFunction, Row r) {
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("instance"), vk::Instance>, "Row must have field named 'instance'");
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("physicalDevice"), vk::PhysicalDevice>, "Row must have field named 'physicalDevice'");
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("device"), vk::Device>, "Row must have field named 'device'");

        vk::Instance instance = boost::hana::at_key(r, BOOST_HANA_STRING("instance"));
        vk::PhysicalDevice physicalDevice = boost::hana::at_key(r, BOOST_HANA_STRING("physicalDevice"));
        vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));

        // create a vmaAllocator
        VmaAllocatorCreateInfo vmaInfo;
        vmaInfo.flags = VmaAllocatorCreateFlags(0);
        vmaInfo.physicalDevice = physicalDevice;
        vmaInfo.device = device;
        vmaInfo.preferredLargeHeapBlockSize = 0; // use default
        vmaInfo.pAllocationCallbacks = nullptr; // maybe add later
        vmaInfo.pDeviceMemoryCallbacks = nullptr;
        vmaInfo.pHeapSizeLimit = nullptr;
        vmaInfo.pVulkanFunctions = nullptr;
        vmaInfo.instance = instance;
        vmaInfo.vulkanApiVersion = VK_API_VERSION_1_2;
#if VMA_EXTERNAL_MEMORY
        vmaInfo.pTypeExternalMemoryHandleTypes = nullptr;
#endif    
        VmaAllocator graphicsAllocator;
        VkResult vmaResult = vmaCreateAllocator(&vmaInfo, &graphicsAllocator);
        if (vmaResult != VK_SUCCESS)
        {
            return tl::make_unexpected("failed to create vma allocator");
        }

        auto rWithAllocator = boost::hana::insert(r,
            boost::hana::make_pair(BOOST_HANA_STRING("vmaAllocator"), graphicsAllocator)
        );
           
        auto result = RowFunction::applyRow(rWithAllocator);

        vmaDestroyAllocator(graphicsAllocator);

        return result;
    }
};

struct StandardDevice {
    using row_tag = RowType::RowWrapperTag;

    template <typename WrappedReturnType>
    using return_type_transformer = WrappedReturnType;

    template <typename RowFunction, typename Row>
    static constexpr RowFunction::return_type wrapRowFunction(RowFunction, Row r) {
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("instance"), vk::Instance>, "Row must have field named 'instance'");
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("physicalDevice"), vk::PhysicalDevice>, "Row must have field named 'physicalDevice'");
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("surface"), vk::SurfaceKHR>, "Row must have field named 'surface'");

        vk::Instance instance = boost::hana::at_key(r, BOOST_HANA_STRING("instance"));
        vk::PhysicalDevice physicalDevice = boost::hana::at_key(r, BOOST_HANA_STRING("physicalDevice"));
        vk::SurfaceKHR surface = boost::hana::at_key(r, BOOST_HANA_STRING("surface"));

        // get the QueueFamilyProperties of the first PhysicalDevice
        std::pmr::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties<std::pmr::polymorphic_allocator<vk::QueueFamilyProperties>>();

        // get the first index into queueFamiliyProperties which supports graphics
        auto graphicsQueueIterator =
            std::ranges::find_if(queueFamilyProperties, [](vk::QueueFamilyProperties const& qfp) { return static_cast<bool>(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });
        uint32_t graphicsQueueFamilyIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueIterator));
        assert(graphicsQueueFamilyIndex < queueFamilyProperties.size());

        fmt::print("Graphics Queue family index={}\n", graphicsQueueFamilyIndex);

        // find a queue to support presentation
        std::optional<uint32_t> presentQueueFamilyIndex;
        for (uint32_t q = 0; q < queueFamilyProperties.size(); q++)
        {
            if (physicalDevice.getSurfaceSupportKHR(q, surface))
            {
                presentQueueFamilyIndex = q;
            }
        }
        assert(presentQueueFamilyIndex.has_value());

        // We look for a graphics queue and present queue. If they end up in the same family, we create one queue. If they
        // are in different families then we create two queues, one for each family.
        float queuePriority = 0.0f;
        std::pmr::vector<vk::DeviceQueueCreateInfo> queues{ vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), graphicsQueueFamilyIndex, 1, &queuePriority) };
        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex.value())
        {
            queues.emplace_back(vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), presentQueueFamilyIndex.value(), 1, &queuePriority));
        }

        // create a Logical Device (finally!)
        std::array<const char*, 0> layers;
        std::pmr::vector<const char*> extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        vk::DeviceCreateInfo deviceInfo(
            vk::DeviceCreateFlags(),
            queues,
            layers,
            extensions
        );
        vk::Device device = physicalDevice.createDevice(deviceInfo);

        //
        // pull out the queues. They might be the same queue
        vk::Queue graphicsQueue;
        vk::Queue presentQueue;
        device.getQueue(graphicsQueueFamilyIndex, 0, &graphicsQueue);
        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex.value())
        {
            presentQueue = graphicsQueue;
        }
        else
        {
            device.getQueue(presentQueueFamilyIndex.value(), 0, &presentQueue);
        }

        auto newFields = boost::hana::make_map(
            boost::hana::make_pair(BOOST_HANA_STRING("device"), device),
            boost::hana::make_pair(BOOST_HANA_STRING("graphicsQueueFamilyIndex"), graphicsQueueFamilyIndex),
            boost::hana::make_pair(BOOST_HANA_STRING("graphicsQueue"), graphicsQueue),
            boost::hana::make_pair(BOOST_HANA_STRING("presentQueueFamilyIndex"), presentQueueFamilyIndex.value()),
            boost::hana::make_pair(BOOST_HANA_STRING("presentQueue"), presentQueue)
        );
        auto rWithDevice = boost::hana::fold_left(r, newFields, boost::hana::insert);
        auto rowResult = RowFunction::applyRow(rWithDevice);

        device.destroy();
        return rowResult;
    }
};

struct CreateGLFWWindowAndSurface {
    using row_tag = RowType::RowWrapperTag;

    template <typename WrappedReturnType>
    using return_type_transformer = WrappedReturnType;

    template <typename RowFunction, typename Row>
    static constexpr RowFunction::return_type wrapRowFunction(RowFunction, Row r) {
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("config"), VulkanContextConfig>, "Row must have field named 'config'");
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("instance"), vk::Instance>, "Row must have field named 'instance'");

        const VulkanContextConfig& config = boost::hana::at_key(r, BOOST_HANA_STRING("config"));
        vk::Instance instance = boost::hana::at_key(r, BOOST_HANA_STRING("instance"));

        assert(glfwVulkanSupported());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(800, 600, config.AppName.c_str(), nullptr, nullptr);

        VkSurfaceKHR surface;
        VkResult err = glfwCreateWindowSurface(instance, window, nullptr, &surface);
        if (err != VK_SUCCESS)
        {
            std::string s;
            fmt::format_to(std::back_inserter(s), "Error from glfwCreateWindowSurface: {}\n", +err);
            return tl::make_unexpected(s);
        }

        auto newFields = boost::hana::make_map(
            boost::hana::make_pair(BOOST_HANA_STRING("glfwWindow"), window),
            boost::hana::make_pair(BOOST_HANA_STRING("surface"), surface)
        );
        auto rWithWindow = boost::hana::fold_left(r, newFields, boost::hana::insert);
        auto rowResult = RowFunction::applyRow(rWithWindow);

        instance.destroySurfaceKHR(vk::SurfaceKHR(surface));
        glfwDestroyWindow(window);

        return rowResult;
    }
};

struct FirstSwapchainPhysicalDevice {
    using row_tag = RowType::RowWrapperTag;

    template <typename WrappedReturnType>
    using return_type_transformer = WrappedReturnType;

    template <typename RowFunction, typename Row>
    static constexpr RowFunction::return_type wrapRowFunction(RowFunction, Row r) {
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("instance"), vk::Instance>, "Row must have field named 'instance'");
        vk::Instance instance = boost::hana::at_key(r, BOOST_HANA_STRING("instance"));

        // enumerate the physicalDevices
        std::pmr::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices<std::pmr::polymorphic_allocator<vk::PhysicalDevice>>();

        // filter out devices that don't support a swapchain
        auto deviceSupportsSwapchain = [](vk::PhysicalDevice device) {
            // check device extensions
            std::pmr::vector<vk::ExtensionProperties> deviceProperties = device.enumerateDeviceExtensionProperties<std::pmr::polymorphic_allocator<vk::ExtensionProperties>>();
            fmt::print("{} device properties supported\n", deviceProperties.size());
            for (auto& prop : deviceProperties)
            {
                fmt::print("property: {}\n", prop.extensionName.operator std::string());
            }
            bool supportsSwapchain =
                (std::ranges::find_if(deviceProperties,
                    [](vk::ExtensionProperties p) {
                        std::string e = p.extensionName;
                        return e == std::string(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
                    }) != deviceProperties.end());
            fmt::print("device supports swapchain = {}\n", supportsSwapchain);
            return supportsSwapchain;
            };

        auto swapchainDevices = std::views::filter(physicalDevices, deviceSupportsSwapchain);
        vk::PhysicalDevice physicalDevice = swapchainDevices.front();

        auto rWithPhysicalDevice = boost::hana::insert(r,
            boost::hana::make_pair(BOOST_HANA_STRING("physicalDevice"), physicalDevice)
            );
        auto wrappedResult = RowFunction::applyRow(rWithPhysicalDevice);

        // we don't have to destroy physical devices!

        return wrappedResult;
    }
};

struct StandardVulkanInstance {
    using row_tag = RowType::RowWrapperTag;

    template <typename WrappedReturnType>
    using return_type_transformer = WrappedReturnType;

    template <typename RowFunction, typename Row>
    static constexpr RowFunction::return_type wrapRowFunction(RowFunction, Row r) {
        static_assert(RowType::has_named_field<Row, BOOST_HANA_STRING("config"), VulkanContextConfig>, "Row must have field named 'config'");
        const VulkanContextConfig& config = boost::hana::at_key(r, BOOST_HANA_STRING("config"));

        // what extensions are required by GLFW?
        uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        fmt::print("{} extensions required by glfw\n", glfwExtensionCount);
        for (uint32_t ix = 0; ix < glfwExtensionCount; ix++) {
            fmt::print(" required: {}\n", glfwExtensions[ix]);
        }

        bng_array<std::string> totalExtensions;
        std::ranges::for_each(config.requiredExtensions, [&](std::string x) { totalExtensions = totalExtensions.push_back(x); });
        std::ranges::for_each(glfwExtensions, glfwExtensions + glfwExtensionCount, [&](auto x) { totalExtensions = totalExtensions.push_back(x); });

        // add in debug layer
        totalExtensions = totalExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        // InstanceCreateInfo need the extensions as raw strings
        std::pmr::vector<const char*> rawExtensionStrings;
        std::ranges::for_each(totalExtensions.begin(), totalExtensions.end(), [&](const std::string& s) { rawExtensionStrings.push_back(s.c_str()); });

        fmt::print("total required extensions:\n");
        for (uint32_t ix = 0; ix < rawExtensionStrings.size(); ix++) {
            fmt::print(" required: {}\n", rawExtensionStrings[ix]);
        }

        // dump list of available extensions
        std::pmr::vector<vk::ExtensionProperties> vulkanExtensions = vk::enumerateInstanceExtensionProperties<std::pmr::polymorphic_allocator<vk::ExtensionProperties>>(nullptr);
        fmt::print("{} extensions supported\n", vulkanExtensions.size());
        for (auto& prop : vulkanExtensions) {
            fmt::print("supported: {}\n", prop.extensionName.operator std::string());
        }

        // check validation layers
        std::pmr::vector<const char*> totalLayers;
        if (config.useValidation) {
            totalLayers.emplace_back("VK_LAYER_KHRONOS_validation");
        }

        std::pmr::vector<vk::LayerProperties> vulkanLayers = vk::enumerateInstanceLayerProperties<std::pmr::polymorphic_allocator<vk::LayerProperties>>();
        auto validationLayers = vulkanLayers
            | std::views::transform([](vk::LayerProperties p) { std::string s = p.layerName; return s; })
            | std::views::filter([](std::string n) { return n == std::string("VK_LAYER_KHRONOS_validation"); });
        if (validationLayers.empty()) {
            fmt::print("Vulkan layers do not contain VK_LAYER_KHRONOS_validation\n");
            return -1;
        }

        // create an Instance
        vk::ApplicationInfo applicationInfo(config.AppName.c_str(), 1, config.EngineName.c_str(), 1, VK_API_VERSION_1_2);
        vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, totalLayers, rawExtensionStrings);
        vk::Instance instance = vk::createInstance(instanceCreateInfo);

        // if we use validation, setup the debug callback
        std::optional<VkDebugUtilsMessengerEXT> messenger;
        if (config.useValidation) {
            vk::DebugUtilsMessengerCreateInfoEXT debugMessengerInfo(
                {},
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
                debugCallback
            );
            VkDebugUtilsMessengerEXT messengerDeposit;
            auto debugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");
            debugUtilsMessengerEXT(instance, &static_cast<VkDebugUtilsMessengerCreateInfoEXT&>(debugMessengerInfo), nullptr, &messengerDeposit);
            messenger = messengerDeposit;
        }

        auto rWithInstance = boost::hana::insert(r,
            boost::hana::make_pair(BOOST_HANA_STRING("instance"), instance)
            );
        auto wrappedResult = RowFunction::applyRow(rWithInstance);

        // clean up debug callback - this has to be done before destroying the instance
        if (messenger.has_value()) {
            auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");
            destroyDebugUtilsMessengerEXT(instance, messenger.value(), nullptr);
        }

        // destroy instance
        instance.destroy();

        return wrappedResult;
    }
};

struct GLFWOuterWrapper {
    using row_tag = RowType::RowWrapperTag;

    template <typename WrappedReturnType>
    using return_type_transformer = WrappedReturnType;

    template <typename RowFunction, typename Row>
    static constexpr RowFunction::return_type wrapRowFunction(RowFunction, Row r) {
        glfwInit();

        auto rowResult = RowFunction::applyRow(r);

        glfwTerminate();

        return rowResult;
    }
};

export
auto createVulkanContext(const VulkanContextConfig& config) -> tl::expected<int, std::string>
{
    auto configRow = boost::hana::make_map(boost::hana::make_pair(BOOST_HANA_STRING("config"), config));

    auto vulkanStages =
        GLFWOuterWrapper()
        | StandardVulkanInstance()
        | FirstSwapchainPhysicalDevice()
        | CreateGLFWWindowAndSurface()
        | StandardDevice() 
        | StandardVMAAllocator() 
        | InvokeInnerCode();

    return vulkanStages.applyRow(configRow);
}

}