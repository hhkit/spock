#include <vk_descriptors.h>

namespace spock {
void DescriptorLayoutBuilder::add_binding(uint32_t binding,
                                          vk::DescriptorType type) {
  vk::DescriptorSetLayoutBinding newbind{binding, type, 1};
  bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear() { bindings.clear(); }

vk::UniqueDescriptorSetLayout
DescriptorLayoutBuilder::build(vk::Device device,
                               vk::ShaderStageFlags shaderStages, void *pNext,
                               vk::DescriptorSetLayoutCreateFlags flags) {
  for (auto &b : bindings)
    b.stageFlags |= shaderStages;

  fmt::println("there are {} bindings", bindings.size());

  return device.createDescriptorSetLayoutUnique(
      vk::DescriptorSetLayoutCreateInfo{flags, bindings, pNext});
}

void DescriptorAllocator::init_pool(vk::Device device, uint32_t maxSets,
                                    std::span<PoolSizeRatio> poolRatios) {
  std::vector<vk::DescriptorPoolSize> poolSizes;
  poolSizes.reserve(poolRatios.size());
  for (PoolSizeRatio ratio : poolRatios) {
    auto count = uint32_t(ratio.ratio * maxSets);
    assert(count > 0);
    poolSizes.push_back(vk::DescriptorPoolSize{ratio.type, count});
  };

  pool = device.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
      vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, maxSets,
      poolSizes});
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