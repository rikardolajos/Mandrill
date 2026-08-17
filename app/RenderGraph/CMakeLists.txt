add_mandrill_executable(RenderGraph)

target_sources(RenderGraph
	PRIVATE
		"RenderGraph.cpp"
)

add_shaders(RenderGraph
	"GBuffer.vert"
	"GBuffer.frag"
	"Fullscreen.vert"
	"Lighting.frag"
	"Blur.frag"
	"Composite.frag"
)
