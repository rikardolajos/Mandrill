# GLFW
message(STATUS "Configure GLFW")
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
add_subdirectory(glfw)

# IMGUI
message(STATUS "Configure IMGUI")
add_library(imgui INTERFACE)
target_include_directories(imgui INTERFACE imgui)
