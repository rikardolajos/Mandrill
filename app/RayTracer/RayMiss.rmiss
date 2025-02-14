#version 460
#extension GL_EXT_ray_tracing : enable

#include "RayPayload.glsl"

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    rayPayload.color = vec3(0.0, 0.0, 0.2);
    rayPayload.normal = vec3(-1.0);
}
