# stb
add_library(StbImage INTERFACE)
add_library(stb::Image ALIAS StbImage)
target_include_directories(StbImage INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/stb>)


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
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)
