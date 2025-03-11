#include <GLFW/glfw3.h>
#include <cassert>
#include <vk_engine.h>

namespace spock {

constexpr bool use_validation_layers = false;
static VulkanEngine *engine{};

struct VulkanEngine::impl {
  GLFWwindow *_window{};
  int _frameNumber{};
  vk::Extent2D _windowExtent{170, 90};

  impl() {
    glfwInit();
    _window = glfwCreateWindow(_windowExtent.width, _windowExtent.height,
                               "Vulkan Engine", nullptr, nullptr);
  }

  ~impl() noexcept {
    glfwDestroyWindow(_window);
    glfwTerminate();
  }
};

VulkanEngine::VulkanEngine() = default;
VulkanEngine::~VulkanEngine() noexcept = default;

void VulkanEngine::init() {
  assert(_pimpl == nullptr && "already initialized");
  assert(engine == nullptr && "engine already initialized");

  engine = this;
  _pimpl = std::make_unique<impl>();
}

void VulkanEngine::destroy() {
  _pimpl.reset();
  engine = nullptr;
}

void VulkanEngine::draw() {
  while (!glfwWindowShouldClose(_pimpl->_window)) {
    glfwPollEvents();
  }
}
} // namespace spock