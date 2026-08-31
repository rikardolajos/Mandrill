#version 460
#extension GL_EXT_ray_tracing : enable

#include "RayPayload.glsl"

// Specialization constant should be generated from scene information
layout (constant_id = 0) const uint VERTEX_COUNT = 1;
layout (constant_id = 1) const uint INDEX_COUNT = 1;
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

layout(set = 1, binding = 1, std430) readonly buffer VertexBuffer {
	Vertex vertices[VERTEX_COUNT];
} vertexBuffer;

layout(set = 1, binding = 2, std430) readonly buffer IndexBuffer {
	uint indices[INDEX_COUNT];
} indexBuffer;

struct InstanceData {
    uint verticesOffset;
    uint indicesOffset;
};

layout(set = 1, binding = 3, std430) readonly buffer InstanceDataBuffer {
	InstanceData instanceDatas[MESH_COUNT];
} instanceDataBuffer;

const uint BASE_COLOR_TEXTURE_BIT = 1 << 0;
const uint METALLIC_ROUGHNESS_TEXTURE_BIT = 1 << 1;
const uint OCCLUSION_TEXTURE_BIT = 1 << 2;
const uint EMISSIVE_TEXTURE_BIT = 1 << 3;
const uint NORMAL_TEXTURE_BIT = 1 << 4;

struct MaterialParams {
    vec3 baseColor;
    float alpha;
    float metallic;
    float roughness;
    float normalScale;
    float occlusionStrength;
    vec3 emission;
    float emissiveStrength;
    float indexOfRefraction;
    float transmission;
    float alphaCutoff;
    uint alphaMode;
    uint hasTexture;
};

struct Material {
    MaterialParams params;
    uint baseColorTextureIndex;
    uint metallicRoughnessTextureIndex;
    uint occlusionTextureIndex;
    uint emissiveTextureIndex;
    uint normalTextureIndex;
    uint _padding0;  // To enforce same size and alignment as host
    uint _padding1;
    uint _padding2;
};

layout(set = 1, binding = 4, std430) readonly buffer MaterialBuffer {
	Material materials[MATERIAL_COUNT];
} materialBuffer;

layout(set = 1, binding = 5) uniform sampler2D textures[TEXTURE_COUNT];

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec3 attribs;

void main()
{
    rayPayload.hitPoint = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Material index is stored in instance custom index
    Material material = materialBuffer.materials[gl_InstanceCustomIndexEXT];

    // Get triangle vertices
    InstanceData data = instanceDataBuffer.instanceDatas[gl_InstanceID];
    uint i0 = indexBuffer.indices[data.indicesOffset + gl_PrimitiveID * 3 + 0];
    uint i1 = indexBuffer.indices[data.indicesOffset + gl_PrimitiveID * 3 + 1];
    uint i2 = indexBuffer.indices[data.indicesOffset + gl_PrimitiveID * 3 + 2];
    Vertex v0 = vertexBuffer.vertices[data.verticesOffset + i0];
    Vertex v1 = vertexBuffer.vertices[data.verticesOffset + i1];
    Vertex v2 = vertexBuffer.vertices[data.verticesOffset + i2];

    vec2 uv = v0.texcoord * bary.x + v1.texcoord * bary.y + v2.texcoord * bary.z;

    rayPayload.color = material.params.baseColor;
    if ((material.params.hasTexture & BASE_COLOR_TEXTURE_BIT) != 0) {
        rayPayload.color *= texture(textures[material.baseColorTextureIndex], uv).rgb;
    }

    vec3 N = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    rayPayload.normal = N;
    if ((material.params.hasTexture & NORMAL_TEXTURE_BIT) != 0) {
        vec3 normal = texture(textures[material.normalTextureIndex], uv).rgb * 2.0 - 1.0;
        normal.xy *= material.params.normalScale;

        vec3 T = normalize(v0.tangent * bary.x + v1.tangent * bary.y + v2.tangent * bary.z);
        vec3 B = normalize(v0.binormal * bary.x + v1.binormal * bary.y + v2.binormal * bary.z);
        mat3 TBN = mat3(T, B, N);
        
        rayPayload.normal = normalize(vec3(TBN * vec3(normal) * gl_WorldToObjectEXT));
    }

    if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
        rayPayload.normal *= -1;
    }
}
