
// 白南瓜 test application main entry point

#include "bainangua.hpp"
#include "expected.hpp" // using tl::expected since this is C++20
#include "RowType.hpp"
#include "tanuki.hpp"
#include "white_pumpkin.hpp"
//#include "task_X.hpp"

#ifdef NDEBUG
#include <windows.h>
#endif

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <format>
#include <iostream>
#include <boost/hana/map.hpp>
#include <boost/hana/hash.hpp>
#include <boost/hana/define_struct.hpp>
#include <coro/coro.hpp>
#include <map>
#include <memory_resource>
#include <vector>

import Commands;
import VulkanContext;
import OneFrame;
import Pipeline;
import PresentationLayer;
import VertexBuffer;
import UniformBuffer;
import DescriptorSets;
import TextureImage;
import ResourceLoader;

void recordCommandBuffer(vk::CommandBuffer buffer, vk::Framebuffer swapChainImage, vk::Extent2D swapChainExtent, const bainangua::PipelineBundle &pipeline, VkBuffer vertexBuffer, VkBuffer indexBuffer, vk::DescriptorSet uboDescriptorSet) {
	vk::CommandBufferBeginInfo beginInfo({}, {});
	buffer.begin(beginInfo);

	std::array<vk::ClearValue, 1> clearColors{ vk::ClearValue() };

	vk::RenderPassBeginInfo renderPassInfo(
		pipeline.renderPass,
		swapChainImage,
		vk::Rect2D({ 0,0 }, swapChainExtent),
		clearColors
	);
	buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

	buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.graphicsPipelines[0]);

	vk::Viewport viewport(
		0.0f,
		0.0f,
		static_cast<float>(swapChainExtent.width),
		static_cast<float>(swapChainExtent.height),
		0.0f,
		1.0f
		);
	buffer.setViewport(0, 1, &viewport);

	vk::Rect2D scissor({ 0,0 }, swapChainExtent);
	buffer.setScissor(0, 1, &scissor);

	vk::Buffer vertexBuffers[] = { vertexBuffer };
	vk::DeviceSize offsets[] = { 0 };
	buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

	buffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

	buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, 1, &uboDescriptorSet,0,nullptr);

	buffer.drawIndexed(static_cast<uint32_t>(bainangua::staticIndices.size()), 1, 0, 0, 0);

	buffer.endRenderPass();

	buffer.end();
}


struct DrawMVPIndexedGeometry {
	using row_tag = RowType::RowFunctionTag;
	using return_type = void;

	template<typename Row>
	constexpr void applyRow(Row r) {
		vk::CommandBuffer buffer = boost::hana::at_key(r, BOOST_HANA_STRING("primaryCommandBuffer"));
		bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));
		auto [vertexBuffer, bufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexedVertexBuffer"));
		auto [indexBuffer, indexBufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexBuffer"));
		std::pmr::vector<vk::DescriptorSet> descriptorSets = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorSets"));
		size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));

		vk::Buffer vertexBuffers[] = { vertexBuffer };
		vk::DeviceSize offsets[] = { 0 };
		buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

		buffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

		buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.pipelineLayout, 0, 1, &(descriptorSets[multiFrameIndex]), 0, nullptr);

		buffer.drawIndexed(static_cast<uint32_t>(bainangua::staticIndices.size()), 1, 0, 0, 0);
	}
};

struct UpdateUniformBuffer {
	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr RowFunction::return_type wrapRowFunction(RowFunction f, Row r) {
		std::pmr::vector<bainangua::UniformBufferBundle> uniformBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("uniformBuffers"));
		size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));
		vk::Extent2D viewportExtent = boost::hana::at_key(r, BOOST_HANA_STRING("viewportExtent"));

		bainangua::updateUniformBuffer(viewportExtent, uniformBuffers[multiFrameIndex]);

		return f.applyRow(r);
	}
};






template <typename Row>
auto renderLoop (Row r) -> bainangua::bng_expected<bool> {
	bainangua::VulkanContext& s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
	std::shared_ptr<bainangua::PresentationLayer> presenterptr = boost::hana::at_key(r, BOOST_HANA_STRING("presenterptr"));
	bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));

	vk::CommandPool commandPool = boost::hana::at_key(r, BOOST_HANA_STRING("commandPool"));
	std::pmr::vector<vk::CommandBuffer> commandBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("commandBuffers"));

	auto [vertexBuffer, bufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("vertexBuffer"));
	auto [indexBuffer, indexBufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexBuffer"));

	//vk::DescriptorPool descriptorPool = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorPool"));
	std::pmr::vector<vk::DescriptorSet> descriptorSets = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorSets"));
	std::pmr::vector<bainangua::UniformBufferBundle> uniformBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("uniformBuffers"));


	size_t multiFrameIndex = 0;

	while (!glfwWindowShouldClose(s.glfwWindow)) {

		tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result> result =
			bainangua::drawOneFrame(s, presenterptr, pipeline, commandBuffers[multiFrameIndex], multiFrameIndex, [&](vk::CommandBuffer commandbuffer, vk::Framebuffer framebuffer) {
				updateUniformBuffer(presenterptr->swapChainExtent2D_, uniformBuffers[multiFrameIndex]);
				recordCommandBuffer(commandbuffer, framebuffer, presenterptr->swapChainExtent2D_, pipeline, vertexBuffer, indexBuffer, descriptorSets[multiFrameIndex]);
				})
			.and_then([&](std::shared_ptr<bainangua::PresentationLayer> newPresenter) {
				presenterptr = newPresenter;

				glfwPollEvents();
				s.endOfFrame();
				multiFrameIndex = (multiFrameIndex + 1) % bainangua::MultiFrameCount;

				return tl::expected<std::shared_ptr<bainangua::PresentationLayer>, vk::Result>(newPresenter);
			});
		if (!result) {
			s.vkDevice.waitIdle();
			return formatVkResultError("error presenting frame", result.error());
		}
	}

	s.vkDevice.waitIdle();

	return 0;
};

struct InvokeRenderLoop {
	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<bool>;

	template<typename Row>
	constexpr bainangua::bng_expected<bool> applyRow(Row r) { return renderLoop(r); }
};





template <typename Key, typename Resource>
using TestResourceStore = bainangua::SingleResourceKey<Key, Resource>;



#ifdef NDEBUG
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
#else
int main()
#endif
{
	// setup a pool arena for memory allocation.
	// Many of the vulkan-hpp calls use the default allocator so we have to call set_default_resource here.
	//
	std::pmr::synchronized_pool_resource default_pmr_resource;
	std::pmr::polymorphic_allocator<> default_pmr_allocator(&default_pmr_resource);
	std::pmr::set_default_resource(&default_pmr_resource);

	auto coro1 = []() -> coro::task<int> {
		co_return 3;
	};

	auto coro2 = [&]() -> coro::task<void> {
		auto c = coro1();
		int r = co_await c;
		std::cout << std::format("co_await result= {}\n", r);
		/*int r2 = co_await c;
		std::cout << std::format("co_await result2 = {}\n", r2);
		int r3 = co_await c;
		std::cout << std::format("co_await result3 = {}\n", r3);*/
	};

	coro::sync_wait(coro2());

	bainangua::bng_expected<bool> programResult =
	bainangua::createVulkanContext(
		bainangua::VulkanContextConfig{
			.AppName = "My Test App",
			.requiredExtensions = {
				VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
			},
#if NDEBUG
			.useValidation = false,
#else
			.useValidation = true,
#endif
			.innerCode = [=](bainangua::VulkanContext& s) -> bainangua::bng_expected<bool> {
				/*auto stageRow = boost::hana::make_map(boost::hana::make_pair(BOOST_HANA_STRING("context"), s));
				auto stages =
					bainangua::PresentationLayerStage()
					| bainangua::TexPipelineStage(SHADER_DIR)
					| bainangua::SimpleGraphicsCommandPoolStage()
					| bainangua::GPUIndexedVertexBufferStage(bainangua::indexedStaticTexVertices)
					| bainangua::GPUIndexBufferStage()
					| bainangua::CreateCombinedDescriptorPoolStage(bainangua::MultiFrameCount)
					| bainangua::CreateCombinedDescriptorSetsStage(bainangua::MultiFrameCount)
					| bainangua::CreateAndLinkUniformBuffersStage()
					| bainangua::FromFileTextureImageStage(TEXTURES_DIR / std::filesystem::path("default.jpg"))
					| bainangua::Basic2DSamplerStage()
					| bainangua::LinkImageToDescriptorsStage()
					| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
					| bainangua::StandardMultiFrameLoop()
					| UpdateUniformBuffer()
					| bainangua::BasicRendering()
					| DrawMVPIndexedGeometry();

				return stages.applyRow(stageRow);*/

				auto loaderDirectory = boost::hana::make_map(
					boost::hana::make_pair(boost::hana::type_c<TestResourceStore<std::string, int>>,[]<typename Resources, typename Storage>(coro::thread_pool& pool, bainangua::ResourceLoader<Resources, Storage>& loader, TestResourceStore<std::string,int> key) -> coro::task<bainangua::bng_expected<int>> {
						co_await pool.schedule();

						std::cout << "int loader running\n";

						auto k1 = TestResourceStore<int, float>{ 1 };
						auto k2 = TestResourceStore<int, float>{ 1 };

						const auto results = co_await coro::when_all(loader.loadResource(k1), loader.loadResource(k2));
						auto result1 = std::get<0>(results).return_value();
						auto result2 = std::get<1>(results).return_value();
						//auto result1 = co_await loader.loadResource(k1);
						//auto result2 = co_await loader.loadResource(k2);

						co_return (result1.and_then([&](auto xval) {
							return result2.transform([&](auto yval) {
								return static_cast<int>(xval+yval);
								});
							})
						);
					}),
					boost::hana::make_pair(boost::hana::type_c<TestResourceStore<int,float>>,[]<typename Resources, typename Storage>(coro::thread_pool& pool, bainangua::ResourceLoader<Resources, Storage>& loader, TestResourceStore<int,float> key) -> coro::task<bainangua::bng_expected<float>> {
						co_await pool.schedule();
						std::cout << "float loader running\n";
						co_return bainangua::bng_expected<float>(3.0f + static_cast<float>(key.key));
					})
				);

				auto loaderStorage = boost::hana::fold_left(
					loaderDirectory,
					boost::hana::make_map(),
					[](auto accumulator, auto v) {
						auto HanaKey = boost::hana::first(v);
						using KeyType = typename decltype(HanaKey)::type;
						using ResourceType = typename decltype(HanaKey)::type::resource_type;

						// we'd like to use unique_ptr here but hana forces a copy somewhere internally
						std::unordered_map<KeyType, std::shared_ptr<bainangua::SingleResourceStore<ResourceType>>> storage;

						return boost::hana::insert(accumulator, boost::hana::make_pair(HanaKey, storage));
					}
				);

				bainangua::bng_expected<bool> result{ true };

				try {
					std::shared_ptr<bainangua::ResourceLoader<decltype(loaderDirectory), decltype(loaderStorage)>> loader(std::make_shared<bainangua::ResourceLoader<decltype(loaderDirectory), decltype(loaderStorage)>>(s, loaderDirectory));
					auto k = TestResourceStore<std::string, int>{ "argh" };
					coro::task<bainangua::bng_expected<int>> loading1 = loader->loadResource(k);
					bainangua::bng_expected<int> result1 = coro::sync_wait(loading1);
					if (result1.has_value()) {
						std::cout << "storage result: " << result1.value() << "\n";
					}
					else {
						std::cout << "result error: " << result1.error() << "\n";
					}

					// this should do cleanup
					/*auto result2 = coro::sync_wait(loading1);
					if (result2.has_value()) {
						std::cout << "int result2: " << std::get<1>(result2.value()) << "\n";
					}
					else {
						std::cout << "result2 error: " << result2.error() << "\n";
					}*/
					//int val = loading2();
					//std::cout << "gentask yield result: " << val << "\n";
				}
				catch (vk::SystemError& err)
				{
					bainangua::bng_errorobject errorString;
					std::format_to(std::back_inserter(errorString), "vk::SystemError: {}", err.what());
					result = tl::make_unexpected(errorString);
				}
				catch (std::exception& err)
				{
					bainangua::bng_errorobject errorString;
					std::format_to(std::back_inserter(errorString), "std::exception: {}", err.what());
					result = tl::make_unexpected(errorString);
				}
				catch (...)
				{
					bainangua::bng_errorobject errorString("unknown error");
					result = tl::make_unexpected(errorString);
				}
				return result;
			}
		}
	);

	if (programResult.has_value()) {
		return 0;
	}
	else {
		std::cout << std::format("program error: {}\n", programResult.error());
		return -1;
	}
}
