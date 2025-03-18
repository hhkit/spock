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

struct DescriptorAllocator {

  struct PoolSizeRatio {
    vk::DescriptorType type;
    float ratio;
  };

  vk::UniqueDescriptorPool pool;

  void init_pool(vk::Device device, uint32_t maxSets,
                 std::span<PoolSizeRatio> poolRatios);
  void clear_descriptors(vk::Device device);

  vk::UniqueDescriptorSet allocate(vk::Device device,
                                   vk::DescriptorSetLayout layout);
};

} // namespace spock