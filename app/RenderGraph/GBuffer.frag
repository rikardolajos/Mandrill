#version 460

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBinormal;
layout(location = 5) in mat3 inNormalMatrix;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

layout(set = 2, binding = 0) uniform MaterialParams {
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
} materialParams;

layout(set = 2, binding = 1) uniform sampler2D baseColorTexture;
layout(set = 2, binding = 2) uniform sampler2D metallicRoughnessTexture;
layout(set = 2, binding = 3) uniform sampler2D occlusionTexture;
layout(set = 2, binding = 4) uniform sampler2D emissiveTexture;
layout(set = 2, binding = 5) uniform sampler2D normalTexture;

const uint BASE_COLOR_TEXTURE_BIT = 1 << 0;
const uint METALLIC_ROUGHNESS_TEXTURE_BIT = 1 << 1;
const uint OCCLUSION_TEXTURE_BIT = 1 << 2;
const uint EMISSIVE_TEXTURE_BIT = 1 << 3;
const uint NORMAL_TEXTURE_BIT = 1 << 4;

const uint ALPHA_MODE_MASK = 1;

void main() {
    vec4 baseColor = vec4(materialParams.baseColor, materialParams.alpha);
    if ((materialParams.hasTexture & BASE_COLOR_TEXTURE_BIT) != 0) {
        baseColor *= texture(baseColorTexture, inTexCoord);
    }

    // Discard cutout parts
    if (materialParams.alphaMode == ALPHA_MODE_MASK && baseColor.a < materialParams.alphaCutoff) {
        discard;
    }

    outPosition = vec4(inWorldPos, 1.0);

    if ((materialParams.hasTexture & NORMAL_TEXTURE_BIT) != 0) {
        mat3 TBN = mat3(normalize(inTangent), normalize(inBinormal), normalize(inNormal));
        vec3 normal = texture(normalTexture, inTexCoord).rgb * 2.0 - 1.0;
        normal.xy *= materialParams.normalScale;
        normal.y *= -1.0; // Normal map for Sponza is in DirectX convention, flip it
        outNormal = vec4(normalize(inNormalMatrix * TBN * normal), 1.0);
    } else {
        outNormal = vec4(normalize(inNormalMatrix * inNormal), 1.0);
    }

    outAlbedo = baseColor;
}
