//
// Code to load/unload shaders via the ResourceLoader.
//

module;

#include "bainangua.hpp"
#include "expected.hpp"
#include "RowType.hpp"
#include "vk_result_to_string.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>
#include <coro/coro.hpp>


export module Shader;

import ResourceLoader;

namespace bainangua {


std::vector<char> readFile(std::filesystem::path filePath)
{
	size_t fileSize = std::filesystem::file_size(filePath);

	std::vector<char> dataBuffer(fileSize);

	std::fstream fs;
	fs.open(filePath, std::ios_base::binary | std::ios_base::in);
	fs.read(dataBuffer.data(), fileSize);
	std::streamsize readCount = fs.gcount();
	fs.close();

	assert((size_t)readCount == fileSize);

	return dataBuffer;
}


vk::ShaderModule createShaderModule(vk::Device device, const std::vector<char>& shaderBytes)
{
	vk::ShaderModuleCreateInfo createInfo(
		vk::ShaderModuleCreateFlags(),
		shaderBytes.size(),
		reinterpret_cast<const uint32_t*>(shaderBytes.data()),
		nullptr
	);

	vk::ShaderModule module = device.createShaderModule(createInfo);
	return module;
}

export
using ShaderFileKey = bainangua::SingleResourceKey<std::filesystem::path, vk::ShaderModule>;

export auto shaderLoader = boost::hana::make_pair(
	boost::hana::type_c<ShaderFileKey>,
	[]<typename Resources, typename Storage>(bainangua::ResourceLoader<Resources, Storage>&loader, ShaderFileKey filekey) -> bainangua::LoaderRoutine<vk::ShaderModule> {
		std::vector<char> shaderCode = readFile(filekey.key);
		vk::ShaderModule shaderModule = createShaderModule(loader.device_, shaderCode);

		co_return bainangua::bng_expected<bainangua::LoaderResults<vk::ShaderModule>>(
			{
				.resource_ = shaderModule,
				.unloader_ = [](vk::Device device, vk::ShaderModule s) -> coro::task<bainangua::bng_expected<void>> {
					device.destroyShaderModule(s);
					co_return{};
				}(loader.device_, shaderModule)
			}
		);
	}
);

export
template <boost::hana::string ShaderName>
struct CreateShaderAsResource {
	CreateShaderAsResource(std::filesystem::path f) : shaderPath(f) {}

	std::filesystem::path shaderPath;

	using row_tag = RowType::RowWrapperTag;

	template <typename WrappedReturnType>
	using return_type_transformer = WrappedReturnType;

	template <typename RowFunction, typename Row>
	constexpr bng_expected<bool> wrapRowFunction(RowFunction f, Row r) {
		vk::Device device = boost::hana::at_key(r, BOOST_HANA_STRING("device"));
		auto loader = boost::hana::at_key(r, BOOST_HANA_STRING("resourceLoader"));

		ShaderFileKey key(shaderPath);

		bng_expected<vk::ShaderModule> shaderModule = coro::sync_wait(loader->loadResource(key));
		if (!shaderModule.has_value()) {
			return bng_unexpected(shaderModule.error());
		}

		auto rWithShader = boost::hana::insert(r, boost::hana::make_pair(ShaderName, shaderModule.value()));
		auto applyResult = f.applyRow(rWithShader);

		coro::sync_wait(loader->unloadResource(key));
		return true;
	}
};

}
