add_mandrill_executable(DeferredShading)

target_sources(DeferredShading
	PRIVATE
		"DeferredShading.cpp"
)

add_shaders(DeferredShading
	"GBuffer.vert"
	"GBuffer.frag"
	"Resolve.vert"
	"Resolve.frag"
)
