add_mandrill_executable(RayTracer)

target_sources(RayTracer
	PRIVATE
		"RayTracer.cpp"
)

add_shaders(RayTracer
	"RayGen.rgen"
	"RayMiss.rmiss"
	"RayClosestHit.rchit"
)
