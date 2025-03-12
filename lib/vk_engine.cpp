#include <cassert>
#include <cstdint>

#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "VkBootstrap.h"
#include <GLFW/glfw3.h>
#include <vk_engine.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace spock {

namespace detail {
struct GLFWDeleter {
  void operator()(GLFWwindow *window) const {
    return glfwDestroyWindow(window);
  }
};
} // namespace detail

constexpr bool use_validation_layers = true;
static VulkanEngine *engine{};
void error_callback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

struct VulkanEngine::impl {
  struct swapchain {
    vk::Extent2D extents;
    vk::SwapchainKHR swapchain;
  };

  std::unique_ptr<GLFWwindow, detail::GLFWDeleter> _window;
  int _frameNumber{};
  vk::Extent2D _windowExtent{1700, 900};

  vk::UniqueInstance _instance;
  vk::UniqueDebugUtilsMessengerEXT _debug_messenger;

  vk::PhysicalDevice _chosen_gpu;
  vk::UniqueDevice _device;
  vk::UniqueSurfaceKHR _surface;

  swapchain _swapchain;

  void init_commands() {}
  void init_sync_structures() {}

  impl() {}

  ~impl() noexcept {
    _surface.reset();
    _device.reset();
    _debug_messenger.reset();
    _instance.reset();
    _window.reset();
  }

  void init_vulkan() {
    glfwSetErrorCallback(+[](int error, const char *desc) {
      fprintf(stderr, "Error: %s\n", desc);
    });

    assert(glfwVulkanSupported());

    // disable opengl context creation
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _window = std::unique_ptr<GLFWwindow, detail::GLFWDeleter>(
        glfwCreateWindow(_windowExtent.width, _windowExtent.height,
                         "Vulkan Engine", nullptr, nullptr));

    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(use_validation_layers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    auto vkb_inst = inst_ret.value();

    _instance.reset(vkb_inst.instance);
    _debug_messenger = vk::UniqueDebugUtilsMessengerEXT(
        vkb_inst.debug_messenger, _instance.get());

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(_instance.get(), _window.get(), nullptr, &surface);
    _surface = vk::UniqueSurfaceKHR{surface, _instance.get()};

    // vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    // vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports vulkan 1.3
    // with the correct features
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice physicalDevice =
        selector.set_minimum_version(1, 3)
            .set_required_features_13(features)
            .set_required_features_12(features12)
            .set_surface(surface)
            .select()
            .value();

    // create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vk::UniqueDevice{vkbDevice.device, _instance};
    _chosen_gpu = physicalDevice.physical_device;
  }

  swapchain create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{_chosen_gpu, _device, _surface};

    vk::Format imageFormat = vk::Format::eB8G8R8A8Unorm;

    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            //.use_default_format_selection()
            .set_desired_format(vk::SurfaceFormatKHR{
                .format = imageFormat,
                .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear})
            // use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();
  }
};

VulkanEngine::VulkanEngine() = default;
VulkanEngine::~VulkanEngine() noexcept = default;

void VulkanEngine::init() {
  assert(_pimpl == nullptr && "already initialized");
  assert(engine == nullptr && "engine already initialized");
  VULKAN_HPP_DEFAULT_DISPATCHER.init();

  engine = this;
  glfwInit();
  _pimpl = std::make_unique<impl>();
  _pimpl->init_vulkan();
  _pimpl->init_commands();
  _pimpl->init_sync_structures();
}

void VulkanEngine::destroy() {
  _pimpl.reset();
  glfwTerminate();
  engine = nullptr;
}

void VulkanEngine::run() {
  glfwSetWindowUserPointer(_pimpl->_window.get(), this);
  glfwSetKeyCallback(
      _pimpl->_window.get(),
      +[](GLFWwindow *window, int key, int scancode, int action, int mods) {
        auto me = static_cast<VulkanEngine *>(glfwGetWindowUserPointer(window));
        fmt::println("{} pressed", key);
      });

  while (!glfwWindowShouldClose(_pimpl->_window.get()))
    draw();
}

void VulkanEngine::draw() {
  // glfwSwapBuffers(_pimpl->_window);
  glfwPollEvents();
}
} // namespace spock