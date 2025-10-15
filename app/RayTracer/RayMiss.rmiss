#version 460
#extension GL_EXT_ray_tracing : enable

#include "RayPayload.glsl"

#define M_PI    3.14159265358979323846
#define M_1_PI  0.318309886183790671538
#define M_1_2PI 0.5 * M_1_PI

layout(set = 2, binding = 0) uniform sampler2D environmentMap;

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

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
    rayPayload.color = texture(environmentMap, worldToLatlongMap(gl_WorldRayDirectionEXT)).rgb;
    rayPayload.normal = vec3(-1.0);
}
