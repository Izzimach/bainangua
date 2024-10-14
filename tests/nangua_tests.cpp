

#include "bainangua.hpp"
#include "gtest/gtest.h"
#include "nangua_tests.hpp"
#include "RowType.hpp"

#include <expected.hpp>
#include <filesystem>

import OneFrame;
import VulkanContext;
import PresentationLayer;
import Pipeline;
import Commands;
import VertexBuffer;

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

auto wrapRenderLoop(std::string_view name, std::function<bool(VulkanContext&)> renderLoop) -> tl::expected<int, std::string> {
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
auto wrapRenderLoopRow(std::string_view name, RowFunction f) -> tl::expected<int, std::string> {
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
		size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));

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
		size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));

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
		size_t multiFrameIndex = boost::hana::at_key(r, BOOST_HANA_STRING("multiFrameIndex"));

		vk::Buffer vertexBuffers[] = { vertexBuffer };
		vk::DeviceSize offsets[] = { 0 };
		buffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);

		buffer.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16);

		buffer.drawIndexed(static_cast<uint32_t>(bainangua::staticIndices.size()), 1, 0, 0, 0);

		buffer.draw(static_cast<uint32_t>(bainangua::staticVertices.size()), 1, 0, 0);
	}
};

TEST(VulkanContext, BasicTest)
{
	EXPECT_NO_THROW(
		wrapRenderLoopRow(
			"Basic Test", 
			NoRenderLoop()
		)
	);
}

TEST(PresentationLayer, BasicTest)
{
	EXPECT_NO_THROW(
		wrapRenderLoopRow(
			"PresentationLayer Pipeline Test App", 
			PresentationLayerStage() | NoRenderLoop()
		)
	);
}

TEST(OneFrame, BasicTest)
{
	EXPECT_EQ(
		wrapRenderLoopRow(
			"OneFrame Pipeline Test App",
			PresentationLayerStage()
			| NoVertexPipelineStage(ShaderPath)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| StandardMultiFrameLoop(10)
			| BasicRendering()
			| DrawNoVertexGeometry()
		),
		(tl::expected<int,std::string>(0))
	);
}

TEST(OneFrame, VertexBuffer)
{
	EXPECT_EQ(
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
		),
		(tl::expected<int, std::string>(0))
	);

}

TEST(OneFrame, IndexBuffer)
{
	EXPECT_EQ(
		wrapRenderLoopRow(
			"IndexBuffer Pipeline Test App",
			PresentationLayerStage()
			| VTVertexPipelineStage(ShaderPath)
			| bainangua::SimpleGraphicsCommandPoolStage()
			| GPUIndexedVertexBufferStage()
			| GPUIndexBufferStage()
			| bainangua::PrimaryGraphicsCommandBuffersStage(bainangua::MultiFrameCount)
			| StandardMultiFrameLoop(10)
			| BasicRendering()
			| DrawIndexedVertexGeometry()
		),
		(tl::expected<int, std::string>(0))
	);
}

}