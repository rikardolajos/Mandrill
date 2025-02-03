#version 460
#extension GL_EXT_ray_tracing : enable

struct RayPayload {
    vec3 barycentricCoordinates;
    uint primitiveIndex;
    uint materialIndex;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec3 attribs;

void main()
{
    rayPayload.barycentricCoordinates = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    rayPayload.primitiveIndex = gl_PrimitiveID;
    rayPayload.materialIndex = gl_InstanceCustomIndexEXT;
}
