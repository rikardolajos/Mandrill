add_mandrill_executable(NeuralNetwork)

target_sources(NeuralNetwork
	PRIVATE
		"NeuralNetwork.cpp"
)

add_shaders(NeuralNetwork
	"Fullscreen.vert"
	"NeuralImage.frag"
)
