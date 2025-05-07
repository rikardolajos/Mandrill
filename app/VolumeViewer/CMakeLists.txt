add_mandrill_executable(VolumeViewer)

target_sources(VolumeViewer
	PRIVATE
		"VolumeViewer.cpp"
)

add_shaders(VolumeViewer
	"Environment.frag"
	"Fullscreen.vert"
	"RayMarcher.frag"
)
