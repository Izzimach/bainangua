
#include "bainangua.hpp"
#include "nangua_tests.hpp"
#include "RowType.hpp"

#include <catch2/catch_test_macros.hpp>

#include <expected.hpp>
#include <filesystem>

import Commands;
import DescriptorSets;
import OneFrame;
import Pipeline;
import PresentationLayer;
import TextureImage;
import UniformBuffer;
import VertexBuffer;
import VulkanContext;

using namespace bainangua;

namespace {

std::filesystem::path ShaderPath = SHADER_DIR;

struct NoRenderLoop {
	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<int, std::pmr::string>;

	template<typename Row>
	constexpr tl::expected<int, std::pmr::string> applyRow(Row r) {
		VulkanContext& s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		s.endOfFrame();
		return 0;
	}
};

auto wrapRenderLoop(std::string_view name, std::function<bool(VulkanContext&)> renderLoop) -> bng_expected<bool> {
	return createVulkanContext(
		VulkanContextConfig{
			.AppName = std::string(name),
			.requiredExtensions = {
					VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
					VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
					VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
			},
			.useValidation = false,
			.innerCode = renderLoop
		}
	);
}

template <typename RowFunction>
auto wrapRenderLoopRow(std::string_view name, RowFunction f) -> bng_expected<bool> {
	return wrapRenderLoop(
		name,
		[&](VulkanContext& s) -> bool {
			auto r = boost::hana::make_map(
				boost::hana::make_pair(BOOST_HANA_STRING("context"), s),
				boost::hana::make_pair(BOOST_HANA_STRING("device"), s.vkDevice)
			);
			auto result = f.applyRow(r);
			return true;
		}
	);
}

struct DrawNoVertexGeometry {
	using row_tag = RowType::RowFunctionTag;
	using return_type = void;

	template<typename Row>
	constexpr void applyRow(Row r) {
		vk::CommandBuffer buffer = boost::hana::at_key(r, BOOST_HANA_STRING("primaryCommandBuffer"));
		bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));
		//size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));

		buffer.draw(3, 1, 0, 0);
	}
};

struct DrawVertexGeometry {
	using row_tag = RowType::RowFunctionTag;
	using return_type = void;

	template<typename Row>
	constexpr void applyRow(Row r) {
		vk::CommandBuffer buffer = boost::hana::at_key(r, BOOST_HANA_STRING("primaryCommandBuffer"));
		bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));
		auto [vertexBuffer, bufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("vertexBuffer"));
		//size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));

		vk::Buffer vertexBuffers[] = { vertexBuffer };
		vk::DeviceSize offsets[] = { 0 };
		buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

		buffer.draw(static_cast<uint32_t>(bainangua::staticVertices.size()), 1, 0, 0);
	}
};

struct DrawIndexedVertexGeometry {
	using row_tag = RowType::RowFunctionTag;
	using return_type = void;

	template<typename Row>
	requires   RowType::has_named_field<Row, BOOST_HANA_STRING("indexBuffer"), std::tuple<vk::Buffer, VmaAllocation>>
			&& RowType::has_named_field<Row, BOOST_HANA_STRING("indexedVertexBuffer"), std::tuple<vk::Buffer, VmaAllocation>>
	constexpr void applyRow(Row r) {
		vk::CommandBuffer buffer = boost::hana::at_key(r, BOOST_HANA_STRING("primaryCommandBuffer"));
		bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));
		auto [vertexBuffer, bufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexedVertexBuffer"));
		auto [indexBuffer, indexBufferMemory] = boost::hana::at_key(r, BOOST_HANA_STRING("indexBuffer"));
		//size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));

		vk::Buffer vertexBuffers[] = { vertexBuffer };
		vk::DeviceSize offsets[] = { 0 };
		buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

		buffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

		buffer.drawIndexed(static_cast<uint32_t>(bainangua::staticIndices.size()), 1, 0, 0, 0);

		buffer.draw(static_cast<uint32_t>(bainangua::staticVertices.size()), 1, 0, 0);
	}
};

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






TEST_CASE("VulkanContext", "[Basic]")
{
	REQUIRE(
		wrapRenderLoopRow("Basic Test", NoRenderLoop()) == bng_expected<bool>(true)
	);
}

TEST_CASE("PresentationLayer", "[Basic]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"PresentationLayer Pipeline Test App", 
			PresentationLayerStage() | NoRenderLoop()
		)
		== bng_expected<bool>(true)
	);
}

TEST_CASE("OneFrame", "[Basic][Rendering]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"OneFrame Pipeline Test App",
			PresentationLayerStage()
			| NoVertexPipelineStage(ShaderPath)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| StandardMultiFrameLoop(10)
			| BasicRendering()
			| DrawNoVertexGeometry()
		)
		== (bng_expected<bool>(true))
	);
}

TEST_CASE("VertexBuffer","[Rendering]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"VertexBuffer Pipeline Test App",
			PresentationLayerStage()
			| VTVertexPipelineStage(ShaderPath)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| GPUVertexBufferStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| StandardMultiFrameLoop(10)
			| BasicRendering()
			| DrawVertexGeometry()
		)
		== (bng_expected<bool>(true))
	);
}

TEST_CASE("IndexBuffer", "[Rendering]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"IndexBuffer Pipeline Test App",
			PresentationLayerStage()
			| VTVertexPipelineStage(ShaderPath)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| bainangua::GPUIndexedVertexBufferStage(bainangua::indexedStaticVertices)
			| GPUIndexBufferStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| StandardMultiFrameLoop(10)
			| BasicRendering()
			| DrawIndexedVertexGeometry()
		)
		== (bng_expected<bool>(true))
	);
}

TEST_CASE("Uniform Buffer Object", "[Rendering]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"IndexBuffer Pipeline Test App",
			bainangua::PresentationLayerStage()
			| bainangua::MVPPipelineStage(SHADER_DIR)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| bainangua::GPUIndexedVertexBufferStage(bainangua::indexedStaticVertices)
			| bainangua::GPUIndexBufferStage()
			| bainangua::CreateSimpleDescriptorPoolStage(vk::DescriptorType::eUniformBuffer, bainangua::MultiFrameCount)
			| bainangua::CreateSimpleDescriptorSetsStage(vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex, bainangua::MultiFrameCount)
			| bainangua::CreateAndLinkUniformBuffersStage()
			| bainangua::FromFileTextureImageStage(TEXTURES_DIR / std::filesystem::path("default.jpg"))
			| bainangua::Basic2DSamplerStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| bainangua::StandardMultiFrameLoop(40)
			| UpdateUniformBuffer()
			| bainangua::BasicRendering()
			| DrawMVPIndexedGeometry()
		)
		== bng_expected<bool>(true)
	);
}

TEST_CASE("Textures", "[Rendering]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"Textured Shader Test App",
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
			| bainangua::StandardMultiFrameLoop(40)
			| UpdateUniformBuffer()
			| bainangua::BasicRendering()
			| DrawMVPIndexedGeometry()
		)
		== true
	);
}

}