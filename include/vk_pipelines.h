#pragma once

#include <vk_types.h>

namespace vkutil {
bool load_shader_module(std::filesystem::path filePath, VkDevice device,
                        VkShaderModule *outShaderModule);
}