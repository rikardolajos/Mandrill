#version 460
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec3 hit_value;
hitAttributeEXT vec3 attribs;

void main()
{
    const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    hit_value = bary;
    hit_value = vec3(log(1.0 + gl_HitTEXT) / 5.0);
}
