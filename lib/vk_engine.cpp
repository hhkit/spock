#include "vk_engine.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VkBootstrap.h"
#include <vk_images.h>
#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

namespace spock {

struct FrameData {
  VkCommandPool _commandPool{};
  VkCommandBuffer _mainCommandBuffer{};
  VkSemaphore _swapchainSemaphore{}, _renderSemaphore{};
  VkFence _renderFence{};
};

constexpr unsigned int FRAME_OVERLAP = 2;

constexpr bool use_validation_layers = true;
static VulkanEngine *engine;

struct VulkanEngine::impl {
  GLFWwindow *window = nullptr;
  glm::ivec2 windowExtent{800, 600};
  bool quit = false;
  bool stop_rendering = false;

  VkInstance _instance;                      // Vulkan library handle
  VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
  VkPhysicalDevice _chosenGPU;               // GPU chosen as the default device
  VkDevice _device;                          // Vulkan device for commands
  VkSurfaceKHR _surface;                     // Vulkan window surface

  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;

  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;
  VkExtent2D _swapchainExtent;

  FrameData _frames[FRAME_OVERLAP]{};
  uint64_t _frameNumber{};

  FrameData &get_current_frame() {
    return _frames[_frameNumber % FRAME_OVERLAP];
  };

  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;

  impl() {
    try {
      init_glfw();
      init_vulkan();
      init_swapchain();
      init_commands();
      init_sync_structures();
    } catch (std::exception e) {
      fmt::println("{}", e.what());
      std::exit(1);
    }
  }

  void run() {
    while (!glfwWindowShouldClose(window) && !quit) {
      glfwPollEvents();

      fmt::println("frame: {}", _frameNumber);

      if (stop_rendering) {
        // throttle the speed to avoid the endless spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      } else {
        draw();
      }
    }
  }

  void draw() {
    //> draw_1
    // wait until the gpu has finished rendering the last frame. Timeout of 1
    // second
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence,
                             true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));
    //< draw_1

    //> draw_2
    // request image from the swapchain
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000,
                                   get_current_frame()._swapchainSemaphore,
                                   nullptr, &swapchainImageIndex));
    //< draw_2

    //> draw_3
    // naming it cmd for shorter writing
    VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // begin the command buffer recording. We will use this command buffer
    // exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // start the command buffer recording
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    //< draw_3
    //
    //> draw_4

    // make the swapchain image into writeable mode before rendering
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);

    // make a clear-color from frame number. This will flash with a 120 frame
    // period.
    VkClearColorValue clearValue;
    float flash = std::abs(std::sin(_frameNumber / 120.f));
    clearValue = {{0.0f, 0.0f, flash, 1.0f}};

    VkImageSubresourceRange clearRange =
        vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    // clear image
    vkCmdClearColorImage(cmd, _swapchainImages[swapchainImageIndex],
                         VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    // make the swapchain image into presentable mode
    vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_GENERAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // finalize the command buffer (we can no longer add commands, but it can
    // now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));
    //< draw_4

    //> draw_5
    // prepare the submission to the queue.
    // we want to wait on the _presentSemaphore, as that semaphore is signaled
    // when the swapchain is ready we will signal the _renderSemaphore, to
    // signal that rendering has finished

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        get_current_frame()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo =
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                      get_current_frame()._renderSemaphore);

    VkSubmitInfo2 submit =
        vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    // submit command buffer to the queue and execute it.
    //  _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit,
                            get_current_frame()._renderFence));
    //< draw_5
    //
    //> draw_6
    // prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as its necessary that drawing commands have finished before the image is
    // displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    _frameNumber++;

    //< draw_6
  }

  ~impl() noexcept {
    for (int i = 0; i < FRAME_OVERLAP; i++) {

      // already written from before
      vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

      // destroy sync objects
      vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
      vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
      vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
    }

    vkDeviceWaitIdle(_device);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
      vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
    }

    destroy_swapchain();

    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyDevice(_device, nullptr);

    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
    vkDestroyInstance(_instance, nullptr);

    if (window) {
      glfwDestroyWindow(window);
      window = nullptr;
    }
    glfwTerminate();
  }

private:
  void init_glfw() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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
          auto new_focus = focused;
          auto old_focused = !self->stop_rendering;

          fmt::println("{}", focused ? "focused" : "unfocused");

          if (new_focus != old_focused && focused) {
            self->destroy_swapchain();
            self->create_swapchain(self->windowExtent.x, self->windowExtent.y);
          }
          self->stop_rendering = !focused;
        });
  }

  void init_vulkan() {
    vkb::InstanceBuilder builder;

    // make the vulkan instance, with basic debug features
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(use_validation_layers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    // grab the instance
    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    glfwCreateWindowSurface(_instance, window, nullptr, &_surface);

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
    uint32_t count;
    const char **extensions = glfwGetRequiredInstanceExtensions(&count);

    std::vector<const char *> exts(extensions, extensions + count);
    for (auto &elem : exts)
      fmt::println("{}", elem);
    vkb::PhysicalDevice physicalDevice =
        selector.set_minimum_version(1, 3)
            .set_required_features_13(features)
            .set_required_features_12(features12)
            .set_surface(_surface)
            // .add_required_extensions(
            //     std::vector<const char *>(extensions, extensions + count))
            // .add_required_extension("VK_KHR_swapchain_maintenance1")
            .select()
            .value();

    // create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    vkb::Device vkbDevice = deviceBuilder.build().value();

    assert(vkbDevice);

    // Get the VkDevice handle used in the rest of a vulkan application
    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

    // use vkbootstrap to get a Graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily =
        vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
  }

  void init_swapchain() {
    // create swapchain
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    fmt::println("winsz: {} {}", width, height);
    create_swapchain(width, height);
  }

  void init_commands() {
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command
    // buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
      VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr,
                                   &_frames[i]._commandPool));

      // allocate the default command buffer that we will use for rendering
      VkCommandBufferAllocateInfo cmdAllocInfo =
          vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

      VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo,
                                        &_frames[i]._mainCommandBuffer));
    }
  }

  void init_sync_structures() {
    // create syncronization structures
    // one fence to control when the gpu has finished rendering the frame,
    // and 2 semaphores to syncronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first
    // frame
    VkFenceCreateInfo fenceCreateInfo =
        vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
      fmt::println("create sync {}, sem {}", i,
                   (void *)_frames[i]._renderSemaphore);
      VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr,
                             &_frames[i]._renderFence));

      VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                                 &_frames[i]._swapchainSemaphore));
      VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                                 &_frames[i]._renderSemaphore));
      fmt::println("          {}, sem {}", i,
                   (void *)_frames[i]._renderSemaphore);
    }
  }

  void create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            //.use_default_format_selection()
            .set_desired_format(VkSurfaceFormatKHR{
                .format = _swapchainImageFormat,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            // use vsync present mode
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    _swapchainExtent = vkbSwapchain.extent;
    // store swapchain and its related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = std::move(vkbSwapchain.get_images().value());
    _swapchainImageViews = std::move(vkbSwapchain.get_image_views().value());
  }

  void destroy_swapchain() {
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    // destroy swapchain resources
    for (int i = 0; i < _swapchainImageViews.size(); i++) {
      vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
  }
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

void VulkanEngine::draw() { self->draw(); }
} // namespace spock
