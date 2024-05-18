add_mandrill_executable(SampleApp)

target_sources(SampleApp
	PRIVATE
		"SampleApp.cpp"
)

configure_file(VertexShader.vert ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/VertexShader.vert COPYONLY)
configure_file(FragmentShader.frag ${MANDRILL_RUNTIME_OUTPUT_DIRECTORY}/FragmentShader.frag COPYONLY)
