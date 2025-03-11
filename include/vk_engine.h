#pragma once
#include <vk_types.h>

namespace spock {
class VulkanEngine {
public:
  VulkanEngine();
  ~VulkanEngine() noexcept;
  void init();
  void destroy();
  void draw();
  void run();

  static VulkanEngine &get();

private:
  struct impl;
  std::unique_ptr<impl> _pimpl;
};

} // namespace spock