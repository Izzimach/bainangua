
#include "nangua_tests.hpp"
#include "RowType.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

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

namespace bainangua_BaseTests {

std::filesystem::path ShaderPath = SHADER_DIR;

struct NoRenderLoop {
	using row_tag = RowType::RowFunctionTag;
	using return_type = tl::expected<int, std::string>;

	template<typename Row>
	constexpr tl::expected<int, std::string> applyRow(Row r) {
		bainangua::VulkanContext& s = boost::hana::at_key(r, BOOST_HANA_STRING("context"));
		s.endOfFrame();
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
		wrapRenderLoopRow("Basic Test", NoRenderLoop()) == bainangua::bng_expected<int>(0)
	);
}

TEST_CASE("PresentationLayer", "[Basic]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"PresentationLayer Pipeline Test App", 
			bainangua::PresentationLayerStage() | NoRenderLoop()
		)
		== bainangua::bng_expected<int>(0)
	);
}

TEST_CASE("OneFrame", "[Basic][Rendering]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"OneFrame Pipeline Test App",
			bainangua::PresentationLayerStage()
			| bainangua::NoVertexPipelineStage(ShaderPath)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| bainangua::StandardMultiFrameLoop(10)
			| bainangua::BasicRendering()
			| DrawNoVertexGeometry()
		)
		== (bainangua::bng_expected<bool>(true))
	);
}

TEST_CASE("VertexBuffer","[Rendering]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"VertexBuffer Pipeline Test App",
			bainangua::PresentationLayerStage()
			| bainangua::VTVertexPipelineStage(ShaderPath)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| bainangua::GPUVertexBufferStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| bainangua::StandardMultiFrameLoop(10)
			| bainangua::BasicRendering()
			| DrawVertexGeometry()
		)
		== (bainangua::bng_expected<bool>(true))
	);
}

TEST_CASE("IndexBuffer", "[Rendering]")
{
	REQUIRE(
		wrapRenderLoopRow(
			"IndexBuffer Pipeline Test App",
			bainangua::PresentationLayerStage()
			| bainangua::VTVertexPipelineStage(ShaderPath)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| bainangua::GPUIndexedVertexBufferStage(bainangua::indexedStaticVertices)
			| bainangua::GPUIndexBufferStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| bainangua::StandardMultiFrameLoop(10)
			| bainangua::BasicRendering()
			| DrawIndexedVertexGeometry()
		)
		== (bainangua::bng_expected<bool>(true))
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
		== bainangua::bng_expected<bool>(true)
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