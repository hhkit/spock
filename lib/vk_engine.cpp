#include <cassert>
#include <cstdint>
#include <optional>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "VkBootstrap.h"
#include <GLFW/glfw3.h>
#include <vk_engine.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

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

struct VulkanEngine::impl {
  struct swapchain {
    vk::Extent2D extents;
    vk::UniqueSwapchainKHR swapchain;
    std::vector<vk::Image> images;
    std::vector<vk::UniqueImageView> image_views;
  };

  std::unique_ptr<GLFWwindow, detail::GLFWDeleter> _window;
  int _frameNumber{};
  vk::Extent2D _windowExtent{1700, 900};

  vk::Instance _instance;
  vk::UniqueDebugUtilsMessengerEXT _debug_messenger;

  vk::PhysicalDevice _chosen_gpu;
  vk::Device _device;
  vk::UniqueSurfaceKHR _surface;

  std::optional<swapchain> _swapchain;

  void init_commands() {}
  void init_sync_structures() {}

  impl() {}

  ~impl() noexcept {
    _swapchain.reset();
    _surface.reset();
    _device.destroy();
    _debug_messenger.reset();
    _instance.destroy();
    _window.reset();
  }

  void init_vulkan() {
    VULKAN_HPP_DEFAULT_DISPATCHER.init();

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
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance{vkb_inst.instance});
    _instance = vkb_inst.instance;
    assert(&VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyInstance);
    assert(&VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyDebugUtilsMessengerEXT);
    _debug_messenger =
        vk::UniqueDebugUtilsMessengerEXT(vkb_inst.debug_messenger, _instance);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(_instance, _window.get(), nullptr, &surface);
    _surface = vk::UniqueSurfaceKHR{surface, _instance};

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
    // VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device{vkbDevice.device});
    // _device = vk::UniqueDevice{vk::Device{vkbDevice.device}, _instance};
    _device = vk::Device{vkbDevice.device};

    _chosen_gpu = physicalDevice.physical_device;
  }

  void init_swapchain() {
    _swapchain = create_swapchain(_windowExtent.width, _windowExtent.height);
  }

  swapchain create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{_chosen_gpu, _device,
                                           _surface.get()};

    vk::Format imageFormat = vk::Format::eB8G8R8A8Unorm;

    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            //.use_default_format_selection()
            .set_desired_format(vk::SurfaceFormatKHR{
                imageFormat, vk::ColorSpaceKHR::eSrgbNonlinear})
            // use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    std::vector<vk::Image> images;
    std::vector<vk::UniqueImageView> image_views;
    for (auto image : vkbSwapchain.get_images().value())
      images.push_back(vk::Image(image));
    for (auto image : vkbSwapchain.get_image_views().value())
      image_views.push_back(vk::UniqueImageView(image));

    return swapchain{.extents = vkbSwapchain.extent,
                     .swapchain =
                         vk::UniqueSwapchainKHR{vkbSwapchain.swapchain},
                     .images = std::move(images),
                     .image_views = std::move(image_views)};
  }
};

VulkanEngine::VulkanEngine() = default;
VulkanEngine::~VulkanEngine() noexcept = default;

void VulkanEngine::init() {
  assert(_pimpl == nullptr && "already initialized");
  assert(engine == nullptr && "engine already initialized");

  engine = this;
  glfwInit();
  _pimpl = std::make_unique<impl>();
  _pimpl->init_vulkan();
  // _pimpl->init_swapchain();
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