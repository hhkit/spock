#include "vk_engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

namespace spock {

namespace detail {
struct GLFWDeleter {
  void operator()(GLFWwindow *window) const {
    return glfwDestroyWindow(window);
  }
};
} // namespace detail

constexpr bool use_validation_layers = true;
static VulkanEngine *engine;

struct VulkanEngine::impl {
  GLFWwindow *window = nullptr;
  glm::ivec2 windowExtent{800, 600};
  bool quit = false;
  bool stop_rendering = false;

  impl() {
    glfwInit();
    window = glfwCreateWindow(windowExtent.x, windowExtent.y, "Vulkan Engine",
                              nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(
        window,
        +[](GLFWwindow *window, int key, int scancode, int action, int mods) {
          auto self = static_cast<impl *>(glfwGetWindowUserPointer(window));
        });
    glfwSetWindowFocusCallback(
        window, +[](GLFWwindow *window, int focused) {
          auto self = static_cast<impl *>(glfwGetWindowUserPointer(window));
          self->stop_rendering = !focused;
        });
  }

  void run() {
    while (!glfwWindowShouldClose(window) && !quit) {
      glfwPollEvents();

      if (stop_rendering) {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
      draw();
    }
  }

  void draw() {}

  ~impl() noexcept { glfwTerminate(); }
};

VulkanEngine::VulkanEngine() = default;
VulkanEngine::~VulkanEngine() noexcept = default;

void VulkanEngine::init() {
  assert(self == nullptr && "already initialized");
  assert(engine == nullptr && "engine already initialized");

  engine = this;
  self = std::make_unique<impl>();
}

void VulkanEngine::destroy() {
  self.reset();
  engine = nullptr;
}

void VulkanEngine::run() { self->run(); }

void VulkanEngine::draw() {}
} // namespace spock
