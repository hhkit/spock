add_subdirectory(shaders)

add_executable(spock 
    driver.cpp
    vk_descriptors.cpp
    vk_engine.cpp
    vk_util.cpp
    ext/stb.cpp
    ext/vma.cpp
    ext/vulkan.cpp
)
add_dependencies(
	spock
	tutorial-shader
)
target_compile_definitions(spock PRIVATE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)
target_include_directories(spock PRIVATE ${spock_SOURCE_DIR}/include)
target_link_libraries(spock
    Vulkan::Vulkan 
    vk-bootstrap::vk-bootstrap 
    GPUOpen::VulkanMemoryAllocator 
    stb::Image
    fmt::fmt
    glfw
    glm::glm)