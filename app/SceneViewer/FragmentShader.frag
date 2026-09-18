#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBinormal;
layout(location = 4) in mat3 inNormalMatrix;

layout(location = 0) out vec4 fragColor;

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

layout(set = 3, binding = 0) uniform sampler2D environmentMap;

layout(push_constant) uniform PushConstant {
    vec3 lineColor;
    int _pad0;
    uint renderMode;
    uint discardOnZeroAlpha;
} pushConstant;

const uint BASE_COLOR_TEXTURE_BIT = 1 << 0;
const uint METALLIC_ROUGHNESS_TEXTURE_BIT = 1 << 1;
const uint OCCLUSION_TEXTURE_BIT = 1 << 2;
const uint EMISSIVE_TEXTURE_BIT = 1 << 3;
const uint NORMAL_TEXTURE_BIT = 1 << 4;

bool hasTexture(uint bit) {
    return (materialParams.hasTexture & bit) != 0;
}

void main() {
    // Base color (default)
    fragColor = vec4(materialParams.baseColor, materialParams.alpha);
    if (hasTexture(BASE_COLOR_TEXTURE_BIT)) {
        fragColor *= texture(baseColorTexture, inTexCoord);
        if (pushConstant.discardOnZeroAlpha == 1 && fragColor.a == 0.0) {
            discard;
        }
    }

    // Metallic, which glTF packs in the blue channel of its combined map
    if (pushConstant.renderMode == 1) {
        float metallic = materialParams.metallic;
        if (hasTexture(METALLIC_ROUGHNESS_TEXTURE_BIT)) {
            metallic *= texture(metallicRoughnessTexture, inTexCoord).b;
        }
        fragColor = vec4(vec3(metallic), 1.0);
    }

    // Roughness, in the green channel of the same map
    if (pushConstant.renderMode == 2) {
        float roughness = materialParams.roughness;
        if (hasTexture(METALLIC_ROUGHNESS_TEXTURE_BIT)) {
            roughness *= texture(metallicRoughnessTexture, inTexCoord).g;
        }
        fragColor = vec4(vec3(roughness), 1.0);
    }

    // Occlusion
    if (pushConstant.renderMode == 3) {
        float occlusion = 1.0;
        if (hasTexture(OCCLUSION_TEXTURE_BIT)) {
            occlusion = texture(occlusionTexture, inTexCoord).r;
        }
        fragColor = vec4(vec3(mix(1.0, occlusion, materialParams.occlusionStrength)), 1.0);
    }

    // Emissive
    if (pushConstant.renderMode == 4) {
        vec3 emission = materialParams.emission * materialParams.emissiveStrength;
        if (hasTexture(EMISSIVE_TEXTURE_BIT)) {
            emission *= texture(emissiveTexture, inTexCoord).rgb;
        }
        fragColor = vec4(emission, 1.0);
    }

    // Index of refraction, remapped from the range dielectrics occupy
    if (pushConstant.renderMode == 5) {
        fragColor = vec4(vec3((materialParams.indexOfRefraction - 1.0) / 1.5), 1.0);
    }

    // Alpha
    if (pushConstant.renderMode == 6) {
        fragColor = vec4(vec3(fragColor.a), 1.0);
    }

    // Normal
    if (pushConstant.renderMode == 7) {
        if (hasTexture(NORMAL_TEXTURE_BIT)) {
            mat3 TBN = mat3(normalize(inTangent), normalize(inBinormal), normalize(inNormal));
            vec3 normal = texture(normalTexture, inTexCoord).rgb * 2.0 - 1.0;
            normal.xy *= materialParams.normalScale;
            fragColor.rgb = normalize(inNormalMatrix * TBN * normal);
        } else {
            fragColor = vec4(inNormal, 1.0);
        }
        fragColor.rgb = fragColor.rgb * 0.5 + 0.5;
    }

    // Texture coordinates
    if (pushConstant.renderMode == 8) {
        fragColor = vec4(inTexCoord, 0.0, 1.0);
    }

    // Line render
    if (pushConstant.renderMode == 9) {
        fragColor = vec4(pushConstant.lineColor, 1.0);
    }
}
