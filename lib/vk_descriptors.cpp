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

} // namespace spock