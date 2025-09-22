
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <vk_types.h>

class VulkanEngine {
public:
  // initializes everything in the engine
  void init();

  // shuts down the engine
  void cleanup();

  // draw loop
  void draw();

  // run main loop
  void run();

  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                            std::span<Vertex> vertices);

  VulkanEngine();
  ~VulkanEngine() noexcept;

private:
  struct impl;
  std::unique_ptr<impl> self;
};