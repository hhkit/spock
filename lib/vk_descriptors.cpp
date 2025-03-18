#include <vk_descriptors.h>

namespace spock {
void DescriptorLayoutBuilder::add_binding(uint32_t binding,
                                          vk::DescriptorType type) {
  vk::DescriptorSetLayoutBinding newbind{binding, type};
  bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear() { bindings.clear(); }

vk::UniqueDescriptorSetLayout
DescriptorLayoutBuilder::build(vk::Device device,
                               vk::ShaderStageFlags shaderStages, void *pNext,
                               vk::DescriptorSetLayoutCreateFlags flags) {
  for (auto &b : bindings)
    b.stageFlags |= shaderStages;

  return device.createDescriptorSetLayoutUnique(
      vk::DescriptorSetLayoutCreateInfo{flags, bindings, pNext});
}

void DescriptorAllocator::init_pool(vk::Device device, uint32_t maxSets,
                                    std::span<PoolSizeRatio> poolRatios) {
  std::vector<vk::DescriptorPoolSize> poolSizes;
  for (PoolSizeRatio ratio : poolRatios) {
    poolSizes.push_back(
        vk::DescriptorPoolSize{ratio.type, uint32_t(ratio.ratio * maxSets)});
  };

  pool = device.createDescriptorPoolUnique(
      vk::DescriptorPoolCreateInfo{{}, maxSets, poolSizes});
}

void DescriptorAllocator::clear_descriptors(vk::Device device) {
  device.resetDescriptorPool(*pool);
}

vk::UniqueDescriptorSet
DescriptorAllocator::allocate(vk::Device device,
                              vk::DescriptorSetLayout layout) {
  vk::DescriptorSetAllocateInfo info{*pool, layout};
  auto ds = device.allocateDescriptorSetsUnique(info);
  return std::move(ds.front());
}

} // namespace spock