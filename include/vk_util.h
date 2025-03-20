#pragma once
#include <vk_types.h>

namespace spock::vkutil {
vk::UniqueShaderModule load_shader_module(std::filesystem::path filePath,
                                          vk::Device dev);
} // namespace spock::vkutil