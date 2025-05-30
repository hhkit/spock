#include <cassert>
#include <cstdint>
#include <optional>

#include <vulkan/vulkan.hpp>

#include "VkBootstrap.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vk_descriptors.h>
#include <vk_engine.h>
#include <vk_util.h>

namespace spock {

namespace detail {
struct GLFWDeleter {
  void operator()(GLFWwindow *window) const {
    return glfwDestroyWindow(window);
  }
};

class UniqueVmaAllocator {
public:
  UniqueVmaAllocator() {}
  explicit UniqueVmaAllocator(VmaAllocatorCreateInfo createInfo) {
    vmaCreateAllocator(&createInfo, &allocator);
  }
  UniqueVmaAllocator(UniqueVmaAllocator &&other) : UniqueVmaAllocator{} {
    std::swap(other.allocator, allocator);
  }
  UniqueVmaAllocator &operator=(UniqueVmaAllocator &&other) {
    std::swap(other.allocator, allocator);
    return *this;
  }
  ~UniqueVmaAllocator() noexcept { vmaDestroyAllocator(allocator); }

  operator VmaAllocator() const noexcept { return allocator; }

private:
  VmaAllocator allocator{};
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

  struct frame_data {
    vk::UniqueCommandPool _commandPool;
    std::vector<vk::UniqueCommandBuffer> _commmandBuffer;

    vk::UniqueSemaphore _swapchainSemaphore, _renderSemaphore;
    vk::UniqueFence _renderFence;
  };

  struct allocated_image {
    vk::Image image;
    vk::UniqueImageView imageView;
    VmaAllocation allocation{};
    VmaAllocator allocator{};
    vk::Extent3D extents;
    vk::Format format;

    allocated_image() noexcept = default;
    allocated_image(VmaAllocator allocator,
                    const VkImageCreateInfo &imageCreateInfo,
                    const VmaAllocationCreateInfo &allocCreateInfo,
                    vk::Device device)
        : allocator{allocator} {
      VkImage img;
      vmaCreateImage(allocator, &imageCreateInfo, &allocCreateInfo, &img,
                     &allocation, nullptr);
      image = vk::Image{img};
      imageView = device.createImageViewUnique(vk::ImageViewCreateInfo{
          {},
          image,
          vk::ImageViewType::e2D,
          format,
          {},
          vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0,
                                    1}});
    }

    allocated_image(allocated_image &&rhs) noexcept
        : image{rhs.image}, imageView{std::move(rhs.imageView)},
          allocation{rhs.allocation}, allocator{rhs.allocator},
          extents{rhs.extents}, format{rhs.format} {
      rhs.allocation = nullptr;
    }

    allocated_image &operator=(allocated_image &&rhs) noexcept {
      std::swap(image, rhs.image);
      std::swap(imageView, rhs.imageView);
      std::swap(allocation, rhs.allocation);
      std::swap(allocator, rhs.allocator);
      std::swap(extents, rhs.extents);
      std::swap(format, rhs.format);
      return *this;
    }

    ~allocated_image() noexcept {
      if (imageView || allocation) {
        assert(imageView && allocation);
        imageView.reset();
        vmaDestroyImage(allocator, image, allocation);
      }
    }
  };

  struct draw_resources {
    allocated_image draw_image;
    vk::Extent2D draw_extent;
  };

  struct descriptor_resources {
    DescriptorAllocator globalDescriptorAllocator;
    vk::UniqueDescriptorSet _drawImageDescriptors;
    vk::UniqueDescriptorSetLayout _drawImageDescriptorLayout;
  };

  struct pipeline_resources {
    vk::UniquePipeline gradient_pipeline;
    vk::UniquePipelineLayout gradient_pipeline_layout;
  };

  static constexpr unsigned int FRAME_OVERLAP = 2;
  std::unique_ptr<GLFWwindow, detail::GLFWDeleter> _window;
  int _frameNumber{};
  vk::Extent2D _windowExtent{800, 600};

  vk::UniqueInstance _instance;
  vk::UniqueDebugUtilsMessengerEXT _debug_messenger;

  vk::PhysicalDevice _chosen_gpu;
  vk::UniqueDevice _device;
  vk::UniqueSurfaceKHR _surface;

  std::optional<swapchain> _swapchain;

  std::array<frame_data, FRAME_OVERLAP> _frames;
  vk::Queue _graphics_queue;
  uint32_t _graphics_queue_family;

  detail::UniqueVmaAllocator _allocator{};

  std::optional<draw_resources> _draw_resources;
  std::optional<descriptor_resources> _descriptor_resources;
  pipeline_resources _pipeline_resources;

  impl() {
    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
  }

  ~impl() noexcept { _device->waitIdle(); }

  void init_vulkan() {
    const auto orphan_destroyer =
        vk::detail::ObjectDestroy<vk::detail::NoParent,
                                  vk::detail::DispatchLoaderDynamic>{
            nullptr, VULKAN_HPP_DEFAULT_DISPATCHER};
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

    if (!inst_ret)
      fmt::report_error(inst_ret.error().message().c_str());

    auto vkb_inst = inst_ret.value();
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Instance{vkb_inst.instance});
    _instance = vk::UniqueInstance{vkb_inst.instance, orphan_destroyer};
    _debug_messenger = vk::UniqueDebugUtilsMessengerEXT(
        vkb_inst.debug_messenger, _instance.get());

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(_instance.get(), _window.get(), nullptr, &surface);
    _surface = vk::UniqueSurfaceKHR{surface, _instance.get()};

    assert(_surface);

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
    features12.shaderFloat16 = true;

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the GLFW surface and supports vulkan 1.3
    // with the correct features
    vkb::PhysicalDeviceSelector selector{vkb_inst};
    auto devRes = selector.set_minimum_version(1, 3)
                      .set_required_features_13(features)
                      .set_required_features_12(features12)
                      .set_surface(surface)
                      .select();
    if (!devRes)
      fmt::report_error(devRes.error().message().c_str());

    vkb::PhysicalDevice physicalDevice = devRes.value();

    // create the final vulkan device
    vkb::DeviceBuilder deviceBuilder{physicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vk::Device{vkbDevice.device});

    // holy crap wtf the default ctor doesn't work
    _device = vk::UniqueDevice{vkbDevice.device, orphan_destroyer};

    _chosen_gpu = physicalDevice.physical_device;

    _graphics_queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphics_queue_family =
        vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    _allocator = detail::UniqueVmaAllocator({
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = _chosen_gpu,
        .device = _device.get(),
        .instance = _instance.get(),
    });
  }

  void init_swapchain() {
    _swapchain = create_swapchain(_windowExtent.width, _windowExtent.height);

    vk::Extent3D drawImageExtent{_windowExtent, 1};

    auto drawImage = allocated_image{
        _allocator,
        vk::ImageCreateInfo{vk::ImageCreateFlagBits::eMutableFormat,
                            vk::ImageType::e2D, vk::Format::eR16G16B16A16Sfloat,
                            drawImageExtent, 1, 1, vk::SampleCountFlagBits::e1,
                            vk::ImageTiling::eOptimal,
                            vk::ImageUsageFlagBits::eTransferSrc |
                                vk::ImageUsageFlagBits::eTransferDst |
                                vk::ImageUsageFlagBits::eStorage |
                                vk::ImageUsageFlagBits::eColorAttachment,
                            vk::SharingMode::eExclusive, 0, 0,
                            vk::ImageLayout::ePreinitialized},
        VmaAllocationCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags =
                VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        },
        _device.get()};
    this->_draw_resources = draw_resources{std::move(drawImage), _windowExtent};
  }

  void init_commands() {
    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command
    // buffers

    for (auto &frame : _frames) {
      frame._commandPool = _device->createCommandPoolUnique(
          {vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
           _graphics_queue_family});

      frame._commmandBuffer =
          _device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
              frame._commandPool.get(), vk::CommandBufferLevel::ePrimary, 1});
    }
  }

  void init_sync_structures() {
    for (auto &frame : _frames) {
      frame._renderFence = _device->createFenceUnique(
          vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled});
      frame._swapchainSemaphore = _device->createSemaphoreUnique({});
      frame._renderSemaphore = _device->createSemaphoreUnique({});
    }
  }

  void init_descriptors() {
    _descriptor_resources.emplace();
    auto &descriptor = *_descriptor_resources;
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        {vk::DescriptorType::eStorageImage, 1}};

    descriptor.globalDescriptorAllocator.init_pool(*_device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
      DescriptorLayoutBuilder builder;
      builder.add_binding(0, vk::DescriptorType::eStorageImage);
      descriptor._drawImageDescriptorLayout =
          builder.build(*_device, vk::ShaderStageFlagBits::eCompute);
    }

    descriptor._drawImageDescriptors =
        descriptor.globalDescriptorAllocator.allocate(
            *_device, *descriptor._drawImageDescriptorLayout);
    assert(descriptor._drawImageDescriptors);
    vk::DescriptorSetLayoutBinding{};
    vk::DescriptorImageInfo imgInfo{
        {}, *_draw_resources->draw_image.imageView, vk::ImageLayout::eGeneral};
    vk::WriteDescriptorSet drawImageWrite{*descriptor._drawImageDescriptors, 0,
                                          0, vk::DescriptorType::eStorageImage,
                                          imgInfo};

    _device->updateDescriptorSets(drawImageWrite, {});
  }

  void init_pipelines() { init_background_pipelines(); }

  void init_background_pipelines() {
    _pipeline_resources.gradient_pipeline_layout =
        _device->createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo{
            {}, *_descriptor_resources->_drawImageDescriptorLayout, {}});

    auto shader = vkutil::load_shader_module(
        std::filesystem::current_path() / "shaders/tutorial.spv", *_device);
    if (!shader) {
      fmt::println("Error building compute shader");
    }

    vk::PipelineShaderStageCreateInfo stageInfo{
        {}, vk::ShaderStageFlagBits::eCompute, *shader, "main"};
    vk::ComputePipelineCreateInfo computePipelineCreateInfo{
        {}, stageInfo, *_pipeline_resources.gradient_pipeline_layout};
    auto res =
        _device->createComputePipelineUnique({}, computePipelineCreateInfo);

    VK_CHECK(res.result);
    _pipeline_resources.gradient_pipeline = std::move(res.value);
  }

  swapchain create_swapchain(uint32_t width, uint32_t height) const {
    vkb::SwapchainBuilder swapchainBuilder{_chosen_gpu, _device.get(),
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
      image_views.push_back(vk::UniqueImageView(image, _device.get()));

    return swapchain{.extents = vkbSwapchain.extent,
                     .swapchain = vk::UniqueSwapchainKHR{vkbSwapchain.swapchain,
                                                         _device.get()},
                     .images = std::move(images),
                     .image_views = std::move(image_views)};
  }

  frame_data &get_current_frame() {
    return _frames[_frameNumber % FRAME_OVERLAP];
  };

  void draw() {
    auto &curr_frame = get_current_frame();
    auto &device = *_device;
    VK_CHECK(device.waitForFences(1, &curr_frame._renderFence.get(), true,
                                  1000000000));
    VK_CHECK(device.resetFences(1, &curr_frame._renderFence.get()));
    auto swapchainImageIndex =
        device.acquireNextImageKHR(_swapchain->swapchain.get(), 1000000000,
                                   curr_frame._swapchainSemaphore.get(), {});
    VK_CHECK(swapchainImageIndex.result);

    auto &cmd_buff = *curr_frame._commmandBuffer[0];
    cmd_buff.reset();
    cmd_buff.begin(vk::CommandBufferBeginInfo{
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    transition_image(cmd_buff, _swapchain->images[swapchainImageIndex.value],
                     vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
    draw_background(cmd_buff);
    transition_image(cmd_buff, _swapchain->images[swapchainImageIndex.value],
                     vk::ImageLayout::eGeneral,
                     vk::ImageLayout::eTransferSrcOptimal);
    copy_image_to_image(cmd_buff, _draw_resources->draw_image.image,
                        _swapchain->images[swapchainImageIndex.value],
                        _draw_resources->draw_extent, _swapchain->extents);
    transition_image(cmd_buff, _swapchain->images[swapchainImageIndex.value],
                     vk::ImageLayout::eTransferDstOptimal,
                     vk::ImageLayout::ePresentSrcKHR);

    cmd_buff.end();

    vk::CommandBufferSubmitInfo cmdInfo{cmd_buff};
    vk::SemaphoreSubmitInfo waitInfo{
        curr_frame._swapchainSemaphore.get(),
        1,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        0,
    };
    vk::SemaphoreSubmitInfo signalInfo{
        curr_frame._renderSemaphore.get(),
        1,
        vk::PipelineStageFlagBits2::eAllGraphics,
        0,
    };

    _graphics_queue.submit2(
        vk::SubmitInfo2{
            {},
            waitInfo,
            cmdInfo,
            signalInfo,
        },
        curr_frame._renderFence.get());

    VK_CHECK(_graphics_queue.presentKHR({
        curr_frame._renderSemaphore.get(),
        _swapchain->swapchain.get(),
        swapchainImageIndex.value,
    }));

    _frameNumber++;
  }

  void transition_image(vk::CommandBuffer cmd, vk::Image image,
                        vk::ImageLayout currentLayout,
                        vk::ImageLayout newLayout) {
    vk::ImageMemoryBarrier2 imageBarrier{
        vk::PipelineStageFlagBits2::eAllCommands,
        vk::AccessFlagBits2::eMemoryWrite,
        vk::PipelineStageFlagBits2::eAllCommands,
        vk::AccessFlagBits2::eMemoryWrite | vk::AccessFlagBits2::eMemoryRead,
        currentLayout,
        newLayout,
        0,
        0,
        image,
        vk::ImageSubresourceRange{
            newLayout == vk::ImageLayout::eDepthAttachmentOptimal
                ? vk::ImageAspectFlagBits::eDepth
                : vk::ImageAspectFlagBits::eColor,
            0,
            vk::RemainingMipLevels,
            0,
            vk::RemainingArrayLayers,
        }};

    cmd.pipelineBarrier2(vk::DependencyInfo{{}, {}, {}, imageBarrier});
  }

  void copy_image_to_image(vk::CommandBuffer cmd, vk::Image src, vk::Image dst,
                           vk::Extent2D srcSize, vk::Extent2D dstSize) {
    vk::ImageBlit2KHR blitRegion{
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        {vk::Offset3D{},
         vk::Offset3D{(int)srcSize.width, (int)srcSize.height, 1}},
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
        {vk::Offset3D{},
         vk::Offset3D{(int)dstSize.width, (int)dstSize.height, 1}},
    };

    cmd.blitImage2(vk::BlitImageInfo2{src, vk::ImageLayout::eTransferSrcOptimal,
                                      dst, vk::ImageLayout::eTransferDstOptimal,
                                      blitRegion, vk::Filter::eLinear});
  }

  void draw_background(vk::CommandBuffer cmd) {
    auto flash = std::abs(std::sin(_frameNumber / 20.f));
    vk::ClearColorValue clearValue{1.f, 0.f, flash, 1.f};

    auto clearRange = vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor,
                                                {},
                                                vk::RemainingMipLevels,
                                                {},
                                                vk::RemainingArrayLayers};
    cmd.clearColorImage(_draw_resources->draw_image.image,
                        vk::ImageLayout::eGeneral, clearValue, clearRange);

    cmd.bindPipeline(vk::PipelineBindPoint::eCompute,
                     *_pipeline_resources.gradient_pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                           *_pipeline_resources.gradient_pipeline_layout, 0,
                           *_descriptor_resources->_drawImageDescriptors, {});
    cmd.dispatch(std::ceil(_draw_resources->draw_extent.width / 16.0),
                 std::ceil(_draw_resources->draw_extent.height / 16.0), 1);
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
  glfwPollEvents();
  _pimpl->draw();
}
} // namespace spock
