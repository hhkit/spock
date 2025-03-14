#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

#include <glm/glm.hpp>

#include <vk_engine.h>

int main() {
  spock::VulkanEngine engine;
  engine.init();
  engine.run();
  engine.destroy();
}