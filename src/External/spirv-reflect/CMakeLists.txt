target_sources(Mandrill PRIVATE
	"spirv_reflect.c"
	"spirv_reflect.h"
 )

 target_compile_definitions(Mandrill PUBLIC SPIRV_REFLECT_USE_SYSTEM_SPIRV_H)
