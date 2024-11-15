target_sources(Mandrill PRIVATE
	"RenderPass.cpp"
	"RenderPass.h"
 )

add_subdirectory(Deferred)
add_subdirectory(Rasterizer)
add_subdirectory(RayTracing)
