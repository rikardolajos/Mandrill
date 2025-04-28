#version 460
#extension GL_EXT_ray_tracing : enable

#include "RayPayload.glsl"

// Specialization constant should be generated from scene information
layout (constant_id = 0) const uint VERTEX_COUNT = 24;
layout (constant_id = 1) const uint INDEX_COUNT = 36;
layout (constant_id = 2) const uint MATERIAL_COUNT = 1;
layout (constant_id = 3) const uint TEXTURE_COUNT = 1;
layout (constant_id = 4) const uint MESH_COUNT = 1;

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 texcoord;
    vec3 tangent;
    vec3 binormal;
    float _padding; // To enforce same size and alignment as host
};

layout(set = 4, binding = 1, std430) readonly buffer VertexBuffer {
	Vertex vertices[VERTEX_COUNT];
};

layout(set = 4, binding = 2, std430) readonly buffer IndexBuffer {
	uint indices[INDEX_COUNT];
};

const uint DIFFUSE_TEXTURE_BIT = 1 << 0;
const uint SPECULAR_TEXTURE_BIT = 1 << 1;
const uint AMBIENT_TEXTURE_BIT = 1 << 2;
const uint EMISSION_TEXTURE_BIT = 1 << 3;
const uint NORMAL_TEXTURE_BIT = 1 << 4;

struct MaterialParams {
    vec3 diffuse;
    float shininess;
    vec3 specular;
    float indexOfRefraction;
    vec3 ambient;
    float opacity;
    vec3 emission;
    uint hasTexture;
};

struct Material {
    MaterialParams params;
    uint diffuseTextureIndex;
    uint specularTextureIndex;
    uint ambientTextureIndex;
    uint emissionTextureIndex;
    uint normalTextureIndex;
    uint _padding0;  // To enforce same size and alignment as host
    uint _padding1;
    uint _padding2;
};

layout(set = 4, binding = 3, std430) readonly buffer MaterialBuffer {
	Material materials[MATERIAL_COUNT];
};

layout(set = 4, binding = 4) uniform sampler2D textures[TEXTURE_COUNT];

struct InstanceData {
    uint verticesOffset;
    uint indicesOffset;
};

layout(set = 4, binding = 5, std430) readonly buffer InstanceDataBuffer {
	InstanceData instanceDatas[MESH_COUNT];
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec3 attribs;

void main()
{
    rayPayload.hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Material index is stored in instance custom index
    Material material = materials[gl_InstanceCustomIndexEXT];

    // Get triangle vertices
    InstanceData data = instanceDatas[gl_InstanceID];
    uint i0 = indices[data.indicesOffset + gl_PrimitiveID * 3 + 0];
    uint i1 = indices[data.indicesOffset + gl_PrimitiveID * 3 + 1];
    uint i2 = indices[data.indicesOffset + gl_PrimitiveID * 3 + 2];
    Vertex v0 = vertices[data.verticesOffset + i0];
    Vertex v1 = vertices[data.verticesOffset + i1];
    Vertex v2 = vertices[data.verticesOffset + i2];

    vec2 uv = v0.texcoord * bary.x + v1.texcoord * bary.y + v2.texcoord * bary.z;

    rayPayload.color = material.params.diffuse;
    if ((material.params.hasTexture & DIFFUSE_TEXTURE_BIT) != 0) {
        rayPayload.color = texture(textures[material.diffuseTextureIndex], uv).rgb;
    }

    vec3 N = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    rayPayload.normal = N;
    if ((material.params.hasTexture & NORMAL_TEXTURE_BIT) != 0) {
        vec3 normal = texture(textures[material.normalTextureIndex], uv).rgb * 2.0 - 1.0;

        vec3 T = normalize(v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z);
        vec3 B = normalize(v0.binormal * bary.x + v1.binormal * bary.y + v2.binormal * bary.z);
        mat3 TBN = mat3(T, B, N);
        
        rayPayload.normal = normalize(vec3(TBN * vec3(normal) * gl_WorldToObjectEXT));
    }

    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
        rayPayload.normal *= -1;
    }
}
