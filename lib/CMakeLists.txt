add_executable(spock 
    driver.cpp
    vma.cpp
    stb.cpp
    vk_engine.cpp
    vulkan.cpp
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