#version 460

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;

layout(constant_id = 0) const int MAX_STEPS = 1000;
layout(constant_id = 1) const float STEP_SIZE = 0.002;
layout(constant_id = 2) const float DENSITY = 1.0;

layout(set = 0, binding = 0) uniform CameraUniformDynamic {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;
} camera;

layout(set = 1, binding = 0) uniform sampler3D volume;

layout(push_constant) uniform PushConstant {
	mat4 model_inv;
	vec3 gridMin;
	float _padding0;
	vec3 gridMax;
	float _padding1;
    vec2 viewPort;
} pushConstant;

void main()
{
	const vec2 pixelCenter = vec2(gl_FragCoord.xy) + vec2(0.5);
	const vec2 inUV = pixelCenter / vec2(pushConstant.viewPort);
	vec2 d = inUV * 2.0 - 1.0;

	vec4 origin = camera.view_inv * vec4(0.0, 0.0, 0.0, 1.0);
	vec4 target = camera.proj_inv * vec4(d.x, d.y, 1.0, 1.0);
	vec4 direction = camera.view_inv * vec4(normalize(target.xyz), 0.0);

	vec4 color = vec4(0.0);
	float t = 0.0;
	for (int i = 0; i < MAX_STEPS && color.a < 1.0; i++) {
		vec3 worldPos = origin.xyz + t * direction.xyz;
		vec3 pos = (pushConstant.model_inv * vec4(worldPos, 1.0)).xyz;
		pos = (pos - pushConstant.gridMin) / (pushConstant.gridMax - pushConstant.gridMin);
		float density = DENSITY * texture(volume, pos).r;
		float alpha = 1.0 - exp(-density * STEP_SIZE);

		vec3 sampleColor = vec3(1.0);
		sampleColor *= alpha;

		color.rgb += (1.0 - color.a) * sampleColor;
		color.a += (1.0 - color.a) * alpha;

		t += STEP_SIZE;
	}

	fragColor = color;
}
