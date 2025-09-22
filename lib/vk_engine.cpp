
#include "vk_engine.h"

#include <vk_descriptors.h>
#include <vk_images.h>
#include <vk_initializers.h>
#include <vk_loader.h>
#include <vk_pipelines.h>
#include <vk_types.h>

#include "VkBootstrap.h"
#include "glm/gtx/transform.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "vk_mem_alloc.h"
#include <array>
#include <chrono>
#include <thread>

struct DeletionQueue {
  std::deque<std::function<void()>> deletors;

  void push_function(std::function<void()> &&function) {
    deletors.push_back(function);
  }

  void flush() {
    // reverse iterate the deletion queue to execute all the functions
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
      (*it)(); // call functors
    }

    deletors.clear();
  }
};

//> framedata
struct FrameData {
  VkSemaphore _swapchainSemaphore;
  VkFence _renderFence;

  VkCommandPool _commandPool;
  VkCommandBuffer _mainCommandBuffer;

  DeletionQueue _deletionQueue;
};

constexpr unsigned int FRAME_OVERLAP = 2;
//< framedata

struct AllocatedImage {
  VkImage image;
  VkImageView imageView;
  VmaAllocation allocation;
  VkExtent3D imageExtent;
  VkFormat imageFormat;
};

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputeEffect {
  const char *name;

  VkPipeline pipeline;
  VkPipelineLayout layout;

  ComputePushConstants data;
};

//> init_fn
constexpr bool bUseValidationLayers = true;

struct VulkanEngine::impl {
  VulkanEngine *_parent{};
  bool _isInitialized{false};
  int _frameNumber{0};

  bool stop_rendering{false};

  VkExtent2D _windowExtent{800, 450};

  GLFWwindow *_window{};

  //> inst_init
  VkInstance _instance;                      // Vulkan library handle
  VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
  VkPhysicalDevice _chosenGPU;               // GPU chosen as the default device
  VkDevice _device;                          // Vulkan device for commands
  VkSurfaceKHR _surface;                     // Vulkan window surface
                                             //< inst_init

  //> queues
  FrameData _frames[FRAME_OVERLAP];

  FrameData &get_current_frame() {
    return _frames[_frameNumber % FRAME_OVERLAP];
  };

  VkQueue _graphicsQueue;
  uint32_t _graphicsQueueFamily;
  //< queues

  //> swap_init
  VkSwapchainKHR _swapchain;
  VkFormat _swapchainImageFormat;

  std::vector<VkImage> _swapchainImages;
  std::vector<VkImageView> _swapchainImageViews;
  std::vector<VkSemaphore> _swapchainSemaphores;
  VkExtent2D _swapchainExtent;
  //< swap_init

  AllocatedImage _drawImage;
  AllocatedImage _depthImage;
  VkExtent2D _drawExtent;

  DescriptorAllocator globalDescriptorAllocator;

  VkDescriptorSet _drawImageDescriptors;
  VkDescriptorSetLayout _drawImageDescriptorLayout;

  VkPipeline _gradientPipeline;
  VkPipelineLayout _gradientPipelineLayout;

  // imgui
  VkFence _immFence;
  VkCommandBuffer _immCommandBuffer;
  VkCommandPool _immCommandPool;

  std::vector<ComputeEffect> backgroundEffects;
  int currentBackgroundEffect{0};

  // chapter 3
  VkPipelineLayout _trianglePipelineLayout;
  VkPipeline _trianglePipeline;

  VkPipelineLayout _meshPipelineLayout;
  VkPipeline _meshPipeline;

  GPUMeshBuffers rectangle;
  std::vector<std::shared_ptr<MeshAsset>> testMeshes;

  DeletionQueue _mainDeletionQueue;
  VmaAllocator _allocator;

  impl(VulkanEngine *engine) : _parent(engine) {}
  ~impl() noexcept;
  void run();
  void draw();

  void init_glfw();
  void init_vulkan();

  void init_swapchain();

  void init_commands();

  void init_sync_structures();

  void init_descriptors();

  void init_pipelines();
  void init_background_pipelines();
  void init_triangle_pipeline();
  void init_mesh_pipeline();

  void init_default_data();

  void init_imgui();

  void immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function);
  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                            std::span<Vertex> vertices);

  void create_swapchain(uint32_t width, uint32_t height);
  void destroy_swapchain();
  AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage,
                                VmaMemoryUsage memoryUsage);
  void destroy_buffer(const AllocatedBuffer &buffer);

  void draw_background(VkCommandBuffer cmd);
  void draw_geometry(VkCommandBuffer cmd);
  void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
};

void VulkanEngine::init() {
  self.reset(new impl{this});

  self->init_glfw();
  self->init_vulkan();
  self->init_swapchain();
  self->init_commands();
  self->init_sync_structures();
  self->init_descriptors();
  self->init_pipelines();
  self->init_imgui();
  self->init_default_data();

  // everything went fine
  self->_isInitialized = true;
}
//< init_fn

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices,
                                        std::span<Vertex> vertices) {
  return self->uploadMesh(indices, vertices);
}

void VulkanEngine::impl::init_glfw() {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  _window = glfwCreateWindow(_windowExtent.width, _windowExtent.height,
                             "Vulkan", nullptr, nullptr);

  glfwSetWindowUserPointer(_window, this);
  glfwSetKeyCallback(
      _window,
      +[](GLFWwindow *window, int key, int scancode, int action, int mods) {
        auto self = static_cast<impl *>(glfwGetWindowUserPointer(window));
      });
  glfwSetWindowFocusCallback(
      _window, +[](GLFWwindow *window, int focused) {
        auto self = static_cast<impl *>(glfwGetWindowUserPointer(window));
        auto new_focus = focused;
        auto old_focused = !self->stop_rendering;

        fmt::println("{}", focused ? "focused" : "unfocused");

        self->stop_rendering = !focused;
      });
}

VulkanEngine::VulkanEngine() : self{new impl{this}} {}
VulkanEngine::~VulkanEngine() noexcept = default;

VulkanEngine::impl::~impl() noexcept {
  if (_isInitialized) {

    // make sure the gpu has stopped doing its things
    vkDeviceWaitIdle(_device);

    for (auto &mesh : testMeshes) {
      destroy_buffer(mesh->meshBuffers.indexBuffer);
      destroy_buffer(mesh->meshBuffers.vertexBuffer);
    }

    for (int i = 0; i < FRAME_OVERLAP; i++) {

      vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

      // destroy sync objects
      vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
      vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
      _frames[i]._deletionQueue.flush();
    }

    _mainDeletionQueue.flush();

    destroy_swapchain();

    vkDestroySurfaceKHR(_instance, _surface, nullptr);

    vkDestroyDevice(_device, nullptr);
    vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
    vkDestroyInstance(_instance, nullptr);

    glfwDestroyWindow(_window);
  }
}

void VulkanEngine::cleanup() { self.reset(); }

void VulkanEngine::draw() { self->draw(); }

void VulkanEngine::impl::draw() {
  glfwSetWindowTitle(_window, fmt::format("frame: {}", _frameNumber).data());

  //> draw_1
  // wait until the gpu has finished rendering the last frame. Timeout of 1
  // second
  VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true,
                           1000000000));
  get_current_frame()._deletionQueue.flush();
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

  // begin the command buffer recording. We will use this command buffer exactly
  // once, so we want to let vulkan know that
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  _drawExtent.width = _drawImage.imageExtent.width;
  _drawExtent.height = _drawImage.imageExtent.height;

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  // transition our main draw image into general layout so we can write into it
  // we will overwrite it all so we dont care about what was the older layout
  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_GENERAL);

  draw_background(cmd);

  vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  draw_geometry(cmd);

  // transition the draw image and the swapchain image into their correct
  // transfer layouts
  vkutil::transition_image(cmd, _drawImage.image,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  // execute a copy from the draw image into the swapchain
  vkutil::copy_image_to_image(cmd, _drawImage.image,
                              _swapchainImages[swapchainImageIndex],
                              _drawExtent, _swapchainExtent);

  // set swapchain image layout to Attachment Optimal so we can draw it
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // draw imgui into the swapchain image
  draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

  // set swapchain image layout to Present so we can draw it
  vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // finalize the command buffer (we can no longer add commands, but it can now
  // be executed)
  VK_CHECK(vkEndCommandBuffer(cmd));

  //> draw_5
  // prepare the submission to the queue.
  // we want to wait on the _presentSemaphore, as that semaphore is signaled
  // when the swapchain is ready we will signal the _swapchainSemaphore, to
  // signal that rendering has finished

  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

  VkSemaphoreSubmitInfo waitInfo[] = {vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      get_current_frame()._swapchainSemaphore)};
  VkSemaphoreSubmitInfo signalInfo[] = {
      vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                    _swapchainSemaphores[swapchainImageIndex]),
  };

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, signalInfo, waitInfo);

  // submit command buffer to the queue and execute it.
  //  _renderFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit,
                          get_current_frame()._renderFence));
  //< draw_5
  //
  //> draw_6
  // prepare present
  // this will put the image we just rendered to into the visible window.
  // we want to wait on the _swapchainSemaphore for that,
  // as its necessary that drawing commands have finished before the image is
  // displayed to the user

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = nullptr;
  presentInfo.pSwapchains = &_swapchain;
  presentInfo.swapchainCount = 1;

  presentInfo.pWaitSemaphores = &_swapchainSemaphores[swapchainImageIndex];
  presentInfo.waitSemaphoreCount = 1;

  presentInfo.pImageIndices = &swapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

  // increase the number of frames drawn
  _frameNumber++;

  //< draw_6
}

void VulkanEngine::run() { self->run(); }

void VulkanEngine::impl::run() {
  // SDL_Event e;
  bool bQuit = false;

  // main loop
  while (!bQuit && !glfwWindowShouldClose(_window)) {
    // Handle events on queue
    glfwPollEvents();

    // do not draw if we are minimized
    if (stop_rendering) {
      // throttle the speed to avoid the endless spinning
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::Begin("background")) {

      ComputeEffect &selected = backgroundEffects[currentBackgroundEffect];

      ImGui::Text("Selected effect: %s", selected.name);

      ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0,
                       backgroundEffects.size() - 1);

      ImGui::ColorEdit4("data1", (float *)&selected.data.data1);
      ImGui::ColorEdit4("data2", (float *)&selected.data.data2);
      ImGui::ColorEdit4("data3", (float *)&selected.data.data3);
      ImGui::ColorEdit4("data4", (float *)&selected.data.data4);
    }
    ImGui::End();

    ImGui::Render();
    draw();
  }
}

void VulkanEngine::impl::init_vulkan() {
  //> init_instance
  vkb::InstanceBuilder builder;

  // make the vulkan instance, with basic debug features
  auto inst_ret = builder.set_app_name("Example Vulkan Application")
                      .request_validation_layers(bUseValidationLayers)
                      .use_default_debug_messenger()
                      .require_api_version(1, 3, 0)
                      .build();

  vkb::Instance vkb_inst = inst_ret.value();

  // grab the instance
  _instance = vkb_inst.instance;
  _debug_messenger = vkb_inst.debug_messenger;

  //< init_instance
  //
  //> init_device
  glfwCreateWindowSurface(_instance, _window, nullptr, &_surface);

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
  vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3)
                                           .set_required_features_13(features)
                                           .set_required_features_12(features12)
                                           .set_surface(_surface)
                                           .select()
                                           .value();

  // create the final vulkan device
  vkb::DeviceBuilder deviceBuilder{physicalDevice};

  vkb::Device vkbDevice = deviceBuilder.build().value();

  // Get the VkDevice handle used in the rest of a vulkan application
  _device = vkbDevice.device;
  _chosenGPU = physicalDevice.physical_device;
  //< init_device

  //> init_queue
  // use vkbootstrap to get a Graphics queue
  _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
  _graphicsQueueFamily =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
  //< init_queue

  // initialize the memory allocator
  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.physicalDevice = _chosenGPU;
  allocatorInfo.device = _device;
  allocatorInfo.instance = _instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &_allocator);

  _mainDeletionQueue.push_function([&]() { vmaDestroyAllocator(_allocator); });
}

void VulkanEngine::impl::init_swapchain() {
  create_swapchain(_windowExtent.width, _windowExtent.height);

  // draw image size will match the window
  VkExtent3D drawImageExtent = {_windowExtent.width, _windowExtent.height, 1};

  // hardcoding the draw format to 32 bit float
  _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  _drawImage.imageExtent = drawImageExtent;

  VkImageUsageFlags drawImageUsages{};
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo rimg_info = vkinit::image_create_info(
      _drawImage.imageFormat, drawImageUsages, drawImageExtent);

  // for the draw image, we want to allocate it from gpu local memory
  VmaAllocationCreateInfo rimg_allocinfo = {};
  rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  rimg_allocinfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // allocate and create the image
  vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image,
                 &_drawImage.allocation, nullptr);

  // build a image-view for the draw image to use for rendering
  VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(
      _drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

  VK_CHECK(
      vkCreateImageView(_device, &rview_info, nullptr, &_drawImage.imageView));

  _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
  _depthImage.imageExtent = drawImageExtent;
  VkImageUsageFlags depthImageUsages{};
  depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  VkImageCreateInfo dimg_info = vkinit::image_create_info(
      _depthImage.imageFormat, depthImageUsages, drawImageExtent);

  // allocate and create the image
  vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image,
                 &_depthImage.allocation, nullptr);

  // build a image-view for the draw image to use for rendering
  VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(
      _depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK(
      vkCreateImageView(_device, &dview_info, nullptr, &_depthImage.imageView));

  // add to deletion queues
  _mainDeletionQueue.push_function([this]() {
    vkDestroyImageView(_device, _drawImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
    vkDestroyImageView(_device, _depthImage.imageView, nullptr);
    vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
  });
}
//< init_swap

//> init_cmd
void VulkanEngine::impl::init_commands() {
  // create a command pool for commands submitted to the graphics queue.
  // we also want the pool to allow for resetting of individual command buffers
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

  // imgui command pool
  VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr,
                               &_immCommandPool));

  // allocate the command buffer for immediate submits
  VkCommandBufferAllocateInfo cmdAllocInfo =
      vkinit::command_buffer_allocate_info(_immCommandPool, 1);

  VK_CHECK(
      vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

  _mainDeletionQueue.push_function(
      [this]() { vkDestroyCommandPool(_device, _immCommandPool, nullptr); });
}
//< init_cmd

//> init_sync
void VulkanEngine::impl::init_sync_structures() {
  // create syncronization structures
  // one fence to control when the gpu has finished rendering the frame,
  // and 2 semaphores to syncronize rendering with swapchain
  // we want the fence to start signalled so we can wait on it on the first
  // frame
  VkFenceCreateInfo fenceCreateInfo =
      vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

  for (int i = 0; i < FRAME_OVERLAP; i++) {
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr,
                           &_frames[i]._renderFence));

    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr,
                               &_frames[i]._swapchainSemaphore));
  }

  VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
  _mainDeletionQueue.push_function(
      [this]() { vkDestroyFence(_device, _immFence, nullptr); });
}
//< init_sync

void VulkanEngine::impl::init_descriptors() {
  // create a descriptor pool that will hold 10 sets with 1 image each
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

  globalDescriptorAllocator.init_pool(_device, 10, sizes);

  // make the descriptor set layout for our compute draw
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _drawImageDescriptorLayout =
        builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  // allocate a descriptor set for our draw image
  _drawImageDescriptors =
      globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

  VkDescriptorImageInfo imgInfo{};
  imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgInfo.imageView = _drawImage.imageView;

  VkWriteDescriptorSet drawImageWrite = {};
  drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  drawImageWrite.pNext = nullptr;

  drawImageWrite.dstBinding = 0;
  drawImageWrite.dstSet = _drawImageDescriptors;
  drawImageWrite.descriptorCount = 1;
  drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  drawImageWrite.pImageInfo = &imgInfo;

  vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

  // make sure both the descriptor allocator and the new layout get cleaned up
  // properly
  _mainDeletionQueue.push_function([&]() {
    globalDescriptorAllocator.destroy_pool(_device);

    vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
  });
}

void VulkanEngine::impl::init_pipelines() {
  init_background_pipelines();
  init_triangle_pipeline();
  init_mesh_pipeline();
}

void VulkanEngine::impl::init_background_pipelines() {
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;

  VkPushConstantRange pushConstant{};
  pushConstant.offset = 0;
  pushConstant.size = sizeof(ComputePushConstants);
  pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  computeLayout.pPushConstantRanges = &pushConstant;
  computeLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr,
                                  &_gradientPipelineLayout));

  VkShaderModule gradientShader;
  if (!vkutil::load_shader_module("shaders/gradient_color.comp.spv", _device,
                                  &gradientShader)) {
    fmt::println("Error when building the compute shader \n");
  }

  VkShaderModule skyShader;
  if (!vkutil::load_shader_module("shaders/sky.comp.spv", _device,
                                  &skyShader)) {
    fmt::println("Error when building the compute shader \n");
  }

  VkPipelineShaderStageCreateInfo stageinfo{};
  stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageinfo.pNext = nullptr;
  stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageinfo.module = gradientShader;
  stageinfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType =
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = _gradientPipelineLayout;
  computePipelineCreateInfo.stage = stageinfo;

  ComputeEffect gradient;
  gradient.layout = _gradientPipelineLayout;
  gradient.name = "gradient";
  gradient.data = {};

  // default colors
  gradient.data.data1 = glm::vec4(1, 0, 0, 1);
  gradient.data.data2 = glm::vec4(0, 0, 1, 1);

  VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &gradient.pipeline));

  // change the shader module only to create the sky shader
  computePipelineCreateInfo.stage.module = skyShader;

  ComputeEffect sky;
  sky.layout = _gradientPipelineLayout;
  sky.name = "sky";
  sky.data = {};
  // default sky parameters
  sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

  VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, nullptr,
                                    &sky.pipeline));

  // add the 2 background effects into the array
  backgroundEffects.push_back(gradient);
  backgroundEffects.push_back(sky);

  // destroy structures properly
  vkDestroyShaderModule(_device, gradientShader, nullptr);
  vkDestroyShaderModule(_device, skyShader, nullptr);
  _mainDeletionQueue.push_function([this, sky, gradient]() {
    vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
    vkDestroyPipeline(_device, sky.pipeline, nullptr);
    vkDestroyPipeline(_device, gradient.pipeline, nullptr);
  });
}

void VulkanEngine::impl::init_triangle_pipeline() {
  VkShaderModule triangleFragShader;
  if (!vkutil::load_shader_module("shaders/colored_triangle.frag.spv", _device,
                                  &triangleFragShader)) {
    fmt::println("Error when building the triangle fragment shader module");
  } else {
    fmt::println("Triangle fragment shader succesfully loaded");
  }

  VkShaderModule triangleVertexShader;
  if (!vkutil::load_shader_module("shaders/colored_triangle.vert.spv", _device,
                                  &triangleVertexShader)) {
    fmt::println("Error when building the triangle vertex shader module");
  } else {
    fmt::println("Triangle vertex shader succesfully loaded");
  }

  // build the pipeline layout that controls the inputs/outputs of the shader
  // we are not using descriptor sets or other systems yet, so no need to use
  // anything other than empty default
  VkPipelineLayoutCreateInfo pipeline_layout_info =
      vkinit::pipeline_layout_create_info();
  VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr,
                                  &_trianglePipelineLayout));

  vkutil::PipelineBuilder pipelineBuilder;

  // use the triangle layout we created
  pipelineBuilder._pipelineLayout = _trianglePipelineLayout;
  // connecting the vertex and pixel shaders to the pipeline
  pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
  // it will draw triangles
  pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  // filled triangles
  pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  // no backface culling
  pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  // no multisampling
  pipelineBuilder.set_multisampling_none();
  // no blending
  pipelineBuilder.disable_blending();
  // no depth testing
  pipelineBuilder.disable_depthtest();

  // connect the image format we will draw into, from draw image
  pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
  pipelineBuilder.set_depth_format(_depthImage.imageFormat);

  // finally build the pipeline
  _trianglePipeline = pipelineBuilder.build_pipeline(_device);

  // clean structures
  vkDestroyShaderModule(_device, triangleFragShader, nullptr);
  vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

  _mainDeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
    vkDestroyPipeline(_device, _trianglePipeline, nullptr);
  });
}

void VulkanEngine::impl::init_mesh_pipeline() {
  VkShaderModule triangleFragShader;
  if (!vkutil::load_shader_module("shaders/colored_triangle.frag.spv", _device,
                                  &triangleFragShader)) {
    fmt::println("Error when building the triangle fragment shader module");
  } else {
    fmt::println("Triangle fragment shader succesfully loaded");
  }

  VkShaderModule triangleVertexShader;
  if (!vkutil::load_shader_module("shaders/colored_triangle_mesh.vert.spv",
                                  _device, &triangleVertexShader)) {
    fmt::println("Error when building the triangle vertex shader module");
  } else {
    fmt::println("Triangle vertex shader succesfully loaded");
  }

  VkPushConstantRange bufferRange{};
  bufferRange.offset = 0;
  bufferRange.size = sizeof(GPUDrawPushConstants);
  bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo pipeline_layout_info =
      vkinit::pipeline_layout_create_info();
  pipeline_layout_info.pPushConstantRanges = &bufferRange;
  pipeline_layout_info.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr,
                                  &_meshPipelineLayout));

  vkutil::PipelineBuilder pipelineBuilder;

  // use the triangle layout we created
  pipelineBuilder._pipelineLayout = _meshPipelineLayout;
  // connecting the vertex and pixel shaders to the pipeline
  pipelineBuilder.set_shaders(triangleVertexShader, triangleFragShader);
  // it will draw triangles
  pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  // filled triangles
  pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  // no backface culling
  pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  // no multisampling
  pipelineBuilder.set_multisampling_none();
  // no blending
  pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

  // connect the image format we will draw into, from draw image
  pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
  pipelineBuilder.set_depth_format(_depthImage.imageFormat);

  // finally build the pipeline
  _meshPipeline = pipelineBuilder.build_pipeline(_device);

  // clean structures
  vkDestroyShaderModule(_device, triangleFragShader, nullptr);
  vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

  _mainDeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
    vkDestroyPipeline(_device, _meshPipeline, nullptr);
  });
}

void VulkanEngine::impl::init_default_data() {
  std::array<Vertex, 4> rect_vertices;

  rect_vertices[0].position = {0.5, -0.5, 0};
  rect_vertices[1].position = {0.5, 0.5, 0};
  rect_vertices[2].position = {-0.5, -0.5, 0};
  rect_vertices[3].position = {-0.5, 0.5, 0};

  rect_vertices[0].color = {0, 0, 0, 1};
  rect_vertices[1].color = {0.5, 0.5, 0.5, 1};
  rect_vertices[2].color = {1, 0, 0, 1};
  rect_vertices[3].color = {0, 1, 0, 1};

  std::array<uint32_t, 6> rect_indices;

  rect_indices[0] = 0;
  rect_indices[1] = 1;
  rect_indices[2] = 2;

  rect_indices[3] = 2;
  rect_indices[4] = 1;
  rect_indices[5] = 3;

  rectangle = uploadMesh(rect_indices, rect_vertices);

  // delete the rectangle data on engine shutdown
  _mainDeletionQueue.push_function([&]() {
    destroy_buffer(rectangle.indexBuffer);
    destroy_buffer(rectangle.vertexBuffer);
  });

  testMeshes = loadGltfMeshes(_parent, "assets/basicmesh.glb").value();
}

void VulkanEngine::impl::init_imgui() {
  // 1: create descriptor pool for IMGUI
  //  the size of the pool is very oversize, but it's copied from imgui demo
  //  itself.
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  VkDescriptorPool imguiPool;
  VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

  // 2: initialize imgui library

  // this initializes the core structures of imgui
  ImGui::CreateContext();

  // this initializes imgui for SDL
  ImGui_ImplGlfw_InitForVulkan(_window, true);

  // this initializes imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = _instance;
  init_info.PhysicalDevice = _chosenGPU;
  init_info.Device = _device;
  init_info.Queue = _graphicsQueue;
  init_info.DescriptorPool = imguiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;

  // dynamic rendering parameters for imgui to use
  init_info.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats =
      &_swapchainImageFormat;

  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  // ImGui_ImplVulkan_CreateFontsTexture();

  // add the destroy the imgui created structures
  _mainDeletionQueue.push_function([=, this]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(_device, imguiPool, nullptr);
  });
}

//> init_swap
void VulkanEngine::impl::create_swapchain(uint32_t width, uint32_t height) {
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
  _swapchainImages = vkbSwapchain.get_images().value();
  _swapchainImageViews = vkbSwapchain.get_image_views().value();
  _swapchainSemaphores = std::vector<VkSemaphore>(_swapchainImages.size());

  for (auto &sem : _swapchainSemaphores) {
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &sem));
  }
}

AllocatedBuffer VulkanEngine::impl::create_buffer(size_t allocSize,
                                                  VkBufferUsageFlags usage,
                                                  VmaMemoryUsage memoryUsage) {
  // allocate buffer
  VkBufferCreateInfo bufferInfo = {.sType =
                                       VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.pNext = nullptr;
  bufferInfo.size = allocSize;

  bufferInfo.usage = usage;

  VmaAllocationCreateInfo vmaallocInfo = {};
  vmaallocInfo.usage = memoryUsage;
  vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  AllocatedBuffer newBuffer;

  // allocate the buffer
  VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
                           &newBuffer.buffer, &newBuffer.allocation,
                           &newBuffer.info));

  return newBuffer;
}

//> destroy_sc
void VulkanEngine::impl::destroy_swapchain() {
  for (auto &elem : _swapchainSemaphores)
    vkDestroySemaphore(_device, elem, nullptr);

  vkDestroySwapchainKHR(_device, _swapchain, nullptr);

  // destroy swapchain resources
  for (int i = 0; i < _swapchainImageViews.size(); i++) {

    vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
  }
}
//< destroy_sc

void VulkanEngine::impl::destroy_buffer(const AllocatedBuffer &buffer) {
  vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

void VulkanEngine::impl::immediate_submit(
    std::function<void(VkCommandBuffer cmd)> &&function) {
  VK_CHECK(vkResetFences(_device, 1, &_immFence));
  VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

  VkCommandBuffer cmd = _immCommandBuffer;

  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, {}, {});

  // submit command buffer to the queue and execute it.
  //  _renderFence will now block until the graphic commands finish execution
  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

  VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

GPUMeshBuffers VulkanEngine::impl::uploadMesh(std::span<uint32_t> indices,
                                              std::span<Vertex> vertices) {
  const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

  GPUMeshBuffers newSurface;

  // create vertex buffer
  newSurface.vertexBuffer = create_buffer(
      vertexBufferSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);

  // find the adress of the vertex buffer
  VkBufferDeviceAddressInfo deviceAdressInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = newSurface.vertexBuffer.buffer};
  newSurface.vertexBufferAddress =
      vkGetBufferDeviceAddress(_device, &deviceAdressInfo);

  // create index buffer
  newSurface.indexBuffer = create_buffer(indexBufferSize,
                                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VMA_MEMORY_USAGE_GPU_ONLY);

  AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize,
                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VMA_MEMORY_USAGE_CPU_ONLY);

  void *data;
  VK_CHECK(vmaMapMemory(_allocator, staging.allocation, &data));

  // copy vertex buffer
  memcpy(data, vertices.data(), vertexBufferSize);
  // copy index buffer
  memcpy((char *)data + vertexBufferSize, indices.data(), indexBufferSize);

  immediate_submit([&](VkCommandBuffer cmd) {
    VkBufferCopy vertexCopy{0};
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = vertexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1,
                    &vertexCopy);

    VkBufferCopy indexCopy{0};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = vertexBufferSize;
    indexCopy.size = indexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1,
                    &indexCopy);
  });

  vmaUnmapMemory(_allocator, staging.allocation);
  destroy_buffer(staging);

  return newSurface;
}

void VulkanEngine::impl::draw_background(VkCommandBuffer cmd) {
  ComputeEffect &effect = backgroundEffects[currentBackgroundEffect];

  // bind the background compute pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

  // bind the descriptor set containing the draw image for the compute
  // pipeline
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          _gradientPipelineLayout, 0, 1, &_drawImageDescriptors,
                          0, nullptr);

  vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                     0, sizeof(ComputePushConstants), &effect.data);
  // execute the compute pipeline dispatch. We are using 16x16 workgroup size
  // so we need to divide by it
  vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0),
                std::ceil(_drawExtent.height / 16.0), 1);
}

void VulkanEngine::impl::draw_geometry(VkCommandBuffer cmd) {
  // begin a render pass  connected to our draw image
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
      _drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
      _depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo =
      vkinit::rendering_info(_windowExtent, &colorAttachment, &depthAttachment);

  vkCmdBeginRendering(cmd, &renderInfo);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);

  // set dynamic viewport and scissor
  VkViewport viewport = {};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = _drawExtent.width;
  viewport.height = _drawExtent.height;
  viewport.minDepth = 0.f;
  viewport.maxDepth = 1.f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = _drawExtent.width;
  scissor.extent.height = _drawExtent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // launch a draw command to draw 3 vertices
  // launch a draw command to draw 3 vertices
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

  GPUDrawPushConstants push_constants;
  push_constants.worldMatrix = glm::mat4{1.f};
  push_constants.vertexBuffer = rectangle.vertexBufferAddress;

  vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(GPUDrawPushConstants), &push_constants);
  vkCmdBindIndexBuffer(cmd, rectangle.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

  glm::mat4 view = glm::translate(glm::vec3{0, 0, -5});
  // camera projection
  glm::mat4 projection = glm::perspective(
      glm::radians(70.f), (float)_drawExtent.width / (float)_drawExtent.height,
      10000.f, 0.1f);

  // invert the Y direction on projection matrix so that we are more similar
  // to opengl and gltf axis
  projection[1][1] *= -1;

  push_constants.worldMatrix = projection * view;
  push_constants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

  vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(GPUDrawPushConstants), &push_constants);
  vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, testMeshes[2]->surfaces[0].count, 1,
                   testMeshes[2]->surfaces[0].startIndex, 0, 0);

  vkCmdEndRendering(cmd);
}

void VulkanEngine::impl::draw_imgui(VkCommandBuffer cmd,
                                    VkImageView targetImageView) {
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
      targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo =
      vkinit::rendering_info(_swapchainExtent, &colorAttachment, nullptr);

  vkCmdBeginRendering(cmd, &renderInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}
