add_library(Mandrill SHARED "App.cpp" "App.h" "Common.h" "Error.cpp" "Error.h" "Log.cpp" "Log.h" "Mandrill.h" "Image.cpp")

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
	PRIVATE
		MANDRILL_DLL
)

target_link_libraries(Mandrill
	PUBLIC
	PRIVATE
		glfw
		imgui
)
