cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("Mandrill" VERSION 2024.8.0)

set(MANDRILL_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

function(add_mandrill_executable TARGET)
    add_executable(${TARGET})
    target_link_libraries(${TARGET} PRIVATE Mandrill)
    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY})
endfunction()

function(add_shaders TARGET)
    set(SHADER_SOURCE_FILES ${ARGN})

    set(COMPILE_COMMANDS)
    set(SHADER_OUTPUTS)

    foreach(SHADER_FILENAME IN LISTS SHADER_SOURCE_FILES)
        set(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILENAME})
        set(OUTPUT "${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/${SHADER_FILENAME}.spv")
        set(DEP "${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/${SHADER_FILENAME}.d")
        
        list(APPEND COMPILE_COMMANDS COMMAND Vulkan::glslc --target-env=vulkan1.3 -MD -MF ${DEP} ${INPUT} -o ${OUTPUT})
        list(APPEND SHADER_OUTPUTS ${OUTPUT})
    endforeach()

    add_custom_target(${TARGET}Shaders ALL
        ${COMPILE_COMMANDS}
        COMMENT "Compiling shaders for ${TARGET}"
        SOURCES ${SHADER_SOURCE_FILES}
        BYPRODUCTS ${SHADER_OUTPUTS}
        DEPFILE ${DEP}
    )
endfunction()

find_package(Vulkan REQUIRED COMPONENTS glslc)
include_directories(${Vulkan_INCLUDE_DIRS})
link_libraries(${Vulkan_LIBRARIES})

include_directories(src)
include_directories(src/External)
include_directories(submodules/glm)

add_subdirectory(submodules)
add_subdirectory(src)
add_subdirectory(app)

configure_file(res/icon.png ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/icon.png COPYONLY)
configure_file(res/missing.png ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/missing.png COPYONLY)
configure_file(res/Roboto.ttf ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/Roboto.ttf COPYONLY)
