add_mandrill_executable(ComputeParticles)

target_sources(ComputeParticles
	PRIVATE
		"ComputeParticles.cpp"
)

add_shaders(ComputeParticles
	"Particles.comp"
	"Particles.vert"
	"Particles.frag"
)
