add_mandrill_executable(HeadlessSample)

target_sources(HeadlessSample
	PRIVATE
		"HeadlessSample.cpp"
)

add_shaders(HeadlessSample
	"VertexShader.vert"
	"FragmentShader.frag"
)
