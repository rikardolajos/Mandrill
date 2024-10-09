add_library(Mandrill SHARED
	"App.cpp"
	"App.h"
	"Buffer.cpp"
	"Buffer.h"
	"Camera.cpp"
	"Camera.h"
	"Common.h"
	"Descriptor.cpp"
	"Descriptor.h"
	"Device.cpp"
	"Device.h"
	"Error.cpp"
	"Error.h"
	"Extension.cpp"
	"Extension.h"
	"Helpers.h"
	"Image.cpp"
	"Image.h"
	"Layout.cpp"
	"Layout.h"
	"Log.cpp"
	"Log.h"
	"Mandrill.h"
	"Pipeline.cpp"
	"Pipeline.h"
	"Sampler.cpp"
	"Sampler.h"
	"Scene.cpp"
	"Scene.h"
	"Shader.cpp"
	"Shader.h"
	"Swapchain.cpp"
	"Swapchain.h"
	"Texture.cpp"
	"Texture.h"
)

set_target_properties(Mandrill PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(Mandrill PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
set_target_properties(Mandrill PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY})

target_compile_definitions(Mandrill
	PUBLIC
		$<$<CONFIG:Release>:NDEBUG>
        $<$<CONFIG:Debug>:_DEBUG>
		# MSVC C++ library.
        $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>
        $<$<CXX_COMPILER_ID:MSVC>:_SCL_SECURE_NO_WARNINGS>
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
		MANDRILL_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		MANDRILL_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		MANDRILL_VERSION_PATCH=${PROJECT_VERSION_PATCH}
	PRIVATE
		MANDRILL_DLL
)

target_link_libraries(Mandrill
	PUBLIC
		${Vulkan_LIBRARIES}
		glfw
)

add_subdirectory(External)
add_subdirectory(RenderPasses)

include_directories(External/imgui)
include_directories(External/stb)
include_directories(External/tinyobjloader)
