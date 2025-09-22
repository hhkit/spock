add_subdirectory(shaders)

add_executable(spock 
    driver.cpp
    vk_descriptors.cpp
    vk_images.cpp
    vk_initializers.cpp
    vk_engine.cpp
    vk_pipelines.cpp
    vk_util.cpp
    ext/stb.cpp
    ext/vma.cpp
    ext/vulkan.cpp
)
add_dependencies(
	spock
	gradient_color_shader
    sky_shader
    colored_triangle_vert
    colored_triangle_frag
)

target_include_directories(spock PRIVATE ${spock_SOURCE_DIR}/include)
target_link_libraries(spock
    Vulkan::Vulkan 
    vk-bootstrap::vk-bootstrap 
    GPUOpen::VulkanMemoryAllocator 
    stb
    fmt::fmt
    glfw
    glm::glm
    imgui 
    imgui::glfw
    imgui::vulkan    
)