#version 460

#define M_PI    3.14159265358979323846
#define M_1_PI  0.318309886183790671538
#define M_1_2PI 0.5 * M_1_PI

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform CameraUniformDynamic {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;
} camera;

layout(set = 1, binding = 0) uniform sampler2D environmentMap;

layout(push_constant) uniform PushConstant {
	mat4 model_inv;
	vec3 gridMin;
    float _padding0;
	vec3 gridMax;
    float _padding1;
    vec2 viewPort;
} pushConstant;


vec2 worldToLatlongMap(vec3 dir)
{
    vec3 p = normalize(dir);
    vec2 uv;
    uv.x = atan(-p.z, p.x) * M_1_2PI + 0.5;
    uv.y = acos(-p.y) * M_1_PI;
    return uv;
}

void main()
{
	const vec2 ray_nds = 2.0 * gl_FragCoord.xy / pushConstant.viewPort - 1.0;
	const vec4 ray_clip = vec4(ray_nds, -1.0, 1.0);
	vec4 ray_view = camera.proj_inv * ray_clip;
	ray_view = vec4(ray_view.xy, -1.0, 0.0);
	vec3 ray_world = (camera.view_inv * ray_view).xyz;
	ray_world = normalize(ray_world);

    fragColor = texture(environmentMap, worldToLatlongMap(ray_world));
}
