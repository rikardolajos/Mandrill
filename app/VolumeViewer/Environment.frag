#version 460

#define M_PI    3.14159265358979323846
#define M_1_PI  0.318309886183790671538
#define M_1_2PI 0.5 * M_1_PI

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;
} camera;

layout(set = 1, binding = 0) uniform sampler2D environmentMap;

// Must match the push constant block in RayMarcher.frag so that the pipeline layouts stay compatible
layout(push_constant) uniform PushConstant {
    mat4 model_inv;
    vec3 gridMin;
    float phaseG;
    vec3 gridMax;
    float envIntensity;
    vec2 viewPort;
    uint frameIndex;
    uint flags;
    uint bouncesAndSamples;
    float albedo;
    uint seed;
    float exposure;
} pushConstant;

const uint FLAG_TONEMAP = 1u << 7;

vec3 tonemapACES(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// The swapchain is a UNORM format, so the sRGB transfer function has to be applied here
vec3 linearToSrgb(vec3 c)
{
    return mix(12.92 * c, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, greaterThan(c, vec3(0.0031308)));
}

vec3 display(vec3 radiance)
{
    radiance *= pushConstant.exposure;
    if ((pushConstant.flags & FLAG_TONEMAP) == 0u) {
        return clamp(radiance, 0.0, 1.0);
    }
    return linearToSrgb(tonemapACES(radiance));
}


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

    vec3 radiance = pushConstant.envIntensity * texture(environmentMap, worldToLatlongMap(ray_world)).rgb;
    fragColor = vec4(display(radiance), 1.0);
}
