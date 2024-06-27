add_mandrill_executable(SampleApp)

target_sources(SampleApp
	PRIVATE
		"SampleApp.cpp"
)

add_shaders(SampleApp
	"VertexShader.vert"
	"FragmentShader.frag"
)
