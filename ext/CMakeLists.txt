# stb

FetchContent_Declare(
    Stb
    GIT_REPOSITORY https://github.com/nothings/stb
    GIT_TAG fede005
)
FetchContent_MakeAvailable(Stb)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})


# VulkanMemoryAllocator
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    GIT_TAG v3.2.1)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# vk-bootstrap
FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        v${Vulkan_VERSION}
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

# glm
FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	1.0.1
)
FetchContent_MakeAvailable(glm)
set(GLM_ENABLE_EXPERIMENTAL ON CACHE BOOL "" FORCE)

# fmt
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt
  GIT_TAG        11.1.4
)
FetchContent_MakeAvailable(fmt)

# glfw
FetchContent_Declare(
    glfw 
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG 3.4)
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# fastgltf
FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf
    GIT_TAG v0.9.0
)
FetchContent_MakeAvailable(fastgltf)

# imgui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.92.3
)
FetchContent_MakeAvailable(imgui)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui INTERFACE ${imgui_SOURCE_DIR})

add_library(imgui_glfw STATIC 
     ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
add_library(imgui::glfw ALIAS imgui_glfw)
target_link_libraries(imgui_glfw PUBLIC glfw)
target_include_directories(imgui_glfw PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

add_library(imgui_vulkan STATIC 
     ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
add_library(imgui::vulkan ALIAS imgui_vulkan)
target_link_libraries(imgui_vulkan PUBLIC Vulkan::Vulkan)
target_include_directories(imgui_vulkan PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
