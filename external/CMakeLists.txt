# GLFW
message(STATUS "Configure GLFW")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# ImGUI
message(STATUS "Configure ImGUI")
add_library(imgui INTERFACE)
target_include_directories(imgui INTERFACE imgui)
