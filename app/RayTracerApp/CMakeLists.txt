add_mandrill_executable(RayTracerApp)

target_sources(RayTracerApp
	PRIVATE
		"RayTracerApp.cpp"
)

add_shaders(RayTracerApp
	"RayGen.rgen"
	"RayMiss.rmiss"
	"RayClosestHit.rchit"
	"Resolve.vert"
	"Resolve.frag"
)
