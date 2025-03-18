#pragma once
#include <vk_types.h>
namespace spock {
struct DescriptorLayoutBuilder {
  std::vector<vk::DescriptorSetLayoutBinding> bindings;

  void add_binding(uint32_t binding, vk::DescriptorType type);
  void clear();

  vk::UniqueDescriptorSetLayout
  build(vk::Device device, vk::ShaderStageFlags shaderStages,
        void *pNext = nullptr, vk::DescriptorSetLayoutCreateFlags flags = {});
};
} // namespace spock