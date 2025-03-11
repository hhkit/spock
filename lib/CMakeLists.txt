add_executable(spock 
    driver.cpp
    vma.cpp
    stb.cpp
    vk_engine.cpp
)
set_property(TARGET spock PROPERTY CXX_STANDARD 23)
target_include_directories(spock PRIVATE ${spock_SOURCE_DIR}/include)
target_link_libraries(spock
    Vulkan::Vulkan 
    vk-bootstrap::vk-bootstrap 
    GPUOpen::VulkanMemoryAllocator 
    stb::Image
    fmt::fmt
    glfw
    glm::glm)