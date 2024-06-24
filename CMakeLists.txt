cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("Mandrill" VERSION 2024.5.0)

set(MANDRILL_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

function(add_mandrill_executable target)
    add_executable(${target})
    target_link_libraries(${target} PRIVATE Mandrill)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY})
endfunction()

find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
link_libraries(${Vulkan_LIBRARIES})

include_directories(src)
include_directories(src/External)
include_directories(submodules/glm)

add_subdirectory(submodules)
add_subdirectory(src)
add_subdirectory(app)

configure_file(res/icon.png ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/icon.png COPYONLY)
configure_file(res/Roboto.ttf ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/Roboto.ttf COPYONLY)
