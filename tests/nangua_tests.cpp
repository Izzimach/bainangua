
#include "nangua_tests.hpp"
#include "RowType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <coroutine>
#include <filesystem>


import Commands;
import DescriptorSets;
import OneFrame;
import Pipeline;
import PresentationLayer;
import TextureImage;
import UniformBuffer;
import VertBuffer;
import VulkanContext;


namespace bainangua_BaseTests {

std::filesystem::path ShaderPath = SHADER_DIR;

struct NoRenderLoop {
	using row_tag = RowType::RowFunctionTag;
	using return_type = bainangua::bng_expected<int>;

	template<typename Row>
	constexpr bainangua::bng_expected<int> applyRow(Row r) {
		std::coroutine_handle<> endOfFrame = boost::hana::at_key(r, BOOST_HANA_STRING("endOfFrameCallback"));
		endOfFrame();
		return 0;
	}
};


struct DrawNoVertexGeometry {
	using row_tag = RowType::RowFunctionTag;
	using return_type = void;

	template<typename Row>
	constexpr void applyRow(Row r) {
		vk::CommandBuffer buffer = boost::hana::at_key(r, BOOST_HANA_STRING("primaryCommandBuffer"));
		bainangua::PipelineBundle pipeline = boost::hana::at_key(r, BOOST_HANA_STRING("pipelineBundle"));

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
		std::vector<vk::DescriptorSet> descriptorSets = boost::hana::at_key(r, BOOST_HANA_STRING("descriptorSets"));
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
		std::vector<bainangua::UniformBufferBundle> uniformBuffers = boost::hana::at_key(r, BOOST_HANA_STRING("uniformBuffers"));
		size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));
		vk::Extent2D viewportExtent = boost::hana::at_key(r, BOOST_HANA_STRING("viewportExtent"));

		bainangua::updateUniformBuffer(viewportExtent, uniformBuffers[multiFrameIndex]);

		return f.applyRow(r);
	}
};



TEST_CASE("VulkanContext", "[Basic]")
{
	REQUIRE(
		(bainangua::QuickCreateContext() | NoRenderLoop()).applyRow(testConfig()) == bainangua::bng_expected<int>(0)
	);
}

TEST_CASE("PresentationLayer", "[Basic]")
{
	auto program =
		bainangua::QuickCreateContext()
		| bainangua::PresentationLayerStage()
		| NoRenderLoop();

	REQUIRE(
		program.applyRow(testConfig())
		== bainangua::bng_expected<int>(0)
	);
}

TEST_CASE("OneFrame", "[Basic][Rendering]")
{
	auto program =
		bainangua::QuickCreateContext()
		| bainangua::PresentationLayerStage()
		| bainangua::NoVertexPipelineStage(ShaderPath)
		| bainangua::SimpleGraphicsCommandPoolStage()
		| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
		| bainangua::StandardMultiFrameLoop(10)
		| bainangua::BasicRendering()
		| DrawNoVertexGeometry();

	REQUIRE(
		program.applyRow(testConfig()) == (bainangua::bng_expected<bool>(true))
	);
}

TEST_CASE("VertexBuffer","[Rendering]")
{
	auto program =
		bainangua::QuickCreateContext()
		| bainangua::PresentationLayerStage()
		| bainangua::VTVertexPipelineStage(ShaderPath)
		| bainangua::SimpleGraphicsCommandPoolStage()
		| bainangua::GPUVertexBufferStage()
		| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
		| bainangua::StandardMultiFrameLoop(10)
		| bainangua::BasicRendering()
		| DrawVertexGeometry();

	REQUIRE(
		program.applyRow(testConfig()) == (bainangua::bng_expected<bool>(true))
	);
}

TEST_CASE("IndexBuffer", "[Rendering]")
{
	auto program =
		bainangua::QuickCreateContext()
		| bainangua::PresentationLayerStage()
		| bainangua::VTVertexPipelineStage(ShaderPath)
		| bainangua::SimpleGraphicsCommandPoolStage()
		| bainangua::GPUIndexedVertexBufferStage(bainangua::indexedStaticVertices)
		| bainangua::GPUIndexBufferStage()
		| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
		| bainangua::StandardMultiFrameLoop(10)
		| bainangua::BasicRendering()
		| DrawIndexedVertexGeometry();

	REQUIRE(
		program.applyRow(testConfig()) == (bainangua::bng_expected<bool>(true))
	);
}

TEST_CASE("Uniform Buffer Object", "[Rendering]")
{
	auto program =
		bainangua::QuickCreateContext()
		| bainangua::PresentationLayerStage()
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
		| DrawMVPIndexedGeometry();

	REQUIRE(
		program.applyRow(testConfig()) == bainangua::bng_expected<bool>(true)
	);
}

TEST_CASE("Textures", "[Rendering]")
{
	auto program =
		bainangua::QuickCreateContext()
		| bainangua::PresentationLayerStage()
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
		| DrawMVPIndexedGeometry();

	REQUIRE(
		program.applyRow(testConfig()) == true
	);
}

}