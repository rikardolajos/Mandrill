cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Remove warning about FindBoost caused by OpenVDB
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project("Mandrill" VERSION 2025.6.0)

# Whether sample apps should be built or not
option(MANDRILL_EXCLUDE_APPS "Exclude apps from build" OFF)

set(MANDRILL_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

function(add_mandrill_executable TARGET)
    add_executable(${TARGET})
    target_link_libraries(${TARGET} PRIVATE Mandrill)
    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY})
    if (UNIX)
        set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME ${TARGET}.bin)
    endif(UNIX)
endfunction()

function(add_shaders TARGET)
    set(SHADER_SOURCE_FILES ${ARGN})

    set(COMPILE_COMMANDS)
    set(SHADER_OUTPUTS)

    foreach(SHADER_FILENAME IN LISTS SHADER_SOURCE_FILES)
        set(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_FILENAME})
        set(OUTPUT "${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/${SHADER_FILENAME}.spv")
        set(DEP "${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/${SHADER_FILENAME}.d")
        
        list(APPEND COMPILE_COMMANDS COMMAND Vulkan::glslc --target-env=vulkan1.4 -MD -MF ${DEP} ${INPUT} -o ${OUTPUT})
        list(APPEND SHADER_OUTPUTS ${OUTPUT})
    endforeach()

    make_directory("${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}")

    add_custom_target(${TARGET}Shaders ALL
        ${COMPILE_COMMANDS}
        COMMENT "Compiling shaders for ${TARGET}"
        SOURCES ${SHADER_SOURCE_FILES}
        BYPRODUCTS ${SHADER_OUTPUTS}
        DEPFILE ${DEP}
    )

    add_dependencies(${TARGET} ${TARGET}Shaders)
endfunction()

find_package(Vulkan REQUIRED COMPONENTS glslc)
include_directories(${Vulkan_INCLUDE_DIRS})
link_libraries(${Vulkan_LIBRARIES})

find_package(OpenVDB QUIET)
if (OpenVDB_FOUND)
    link_libraries(OpenVDB::openvdb)
    add_compile_definitions(MANDRILL_USE_OPENVDB)
else()
    message(WARNING
        "OpenVDB was not found in your system. Loading volumes (Texture3D) will not be supported in Mandrill.\n"
        "OpenVDB can be install with vcpkg: `vcpkg install openvdb`")
endif()

include_directories(src)
include_directories(src/External)
include_directories(submodules/glm)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(submodules)
add_subdirectory(src)

if (NOT MANDRILL_EXCLUDE_APPS)
    add_subdirectory(app)
endif()

configure_file(res/icon.png ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/icon.png COPYONLY)
configure_file(res/Roboto.ttf ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/Roboto.ttf COPYONLY)

# Generate documentation
option(MANDRILL_BUILD_DOC "Build documentation" ON)
configure_file(doc/doxygen-awesome.css ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen-awesome.css COPYONLY)
configure_file(doc/doxygen-awesome-sidebar-only.css ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen-awesome-sidebar-only.css COPYONLY)

if (MANDRILL_BUILD_DOC)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)

        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile)

        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        message("Doxygen build started")

        add_custom_target(doc ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    else (DOXYGEN_FOUND)
      message("Doxygen need to be installed to generate the doxygen documentation")
    endif (DOXYGEN_FOUND)
endif(MANDRILL_BUILD_DOC)
