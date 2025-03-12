#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <vulkan.h>

#include <glm/glm.hpp>

#include <vk_engine.h>

int main() {
  init_vulkan();

  spock::VulkanEngine engine;
  engine.init();
  engine.run();
  engine.destroy();
}