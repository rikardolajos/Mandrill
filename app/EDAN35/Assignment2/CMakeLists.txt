add_mandrill_executable(Assignment2)

target_sources(Assignment2
	PRIVATE
		"Assignment2.cpp"
)

add_shaders(Assignment2
	"GBuffer.vert"
	"GBuffer.frag"
	"Resolve.vert"
	"Resolve.frag"
)
