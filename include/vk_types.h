#pragma once

#include <array>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_to_string.hpp>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#define VK_CHECK(x)                                                            \
  do {                                                                         \
    vk::Result err = x;                                                        \
    if (err != vk::Result::eSuccess) {                                         \
      fmt::print("Detected Vulkan error: {}", vk::to_string(err));             \
      abort();                                                                 \
    }                                                                          \
  } while (0)
