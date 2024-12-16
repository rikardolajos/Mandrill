#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inTangent;
layout(location = 3) in vec3 inBinormal;
layout(location = 4) in mat3 inNormalMatrix;

layout(location = 0) out vec4 fragColor;

layout(set = 2, binding = 0) uniform MaterialParams {
    vec3 diffuse;
    float shininess;
    vec3 specular;
    float indexOfRefraction;
    vec3 ambient;
    float opacity;
    vec3 emission;
    uint hasTexture;
} materialParams;

layout(set = 2, binding = 1) uniform sampler2D diffuseTexture;
layout(set = 2, binding = 2) uniform sampler2D specularTexture;
layout(set = 2, binding = 3) uniform sampler2D ambientTexture;
layout(set = 2, binding = 4) uniform sampler2D emissionTexture;
layout(set = 2, binding = 5) uniform sampler2D normalTexture;

layout(push_constant) uniform PushConstant {
    uint renderMode;
    uint discardOnZeroAlpha;
    vec3 lineColor;
} pushConstant;

const uint DIFFUSE_TEXTURE_BIT  = 1 << 0;
const uint SPECULAR_TEXTURE_BIT = 1 << 1;
const uint AMBIENT_TEXTURE_BIT = 1 << 2;
const uint EMISSION_TEXTURE_BIT = 1 << 3;
const uint NORMAL_TEXTURE_BIT = 1 << 4;

void main() {
    // Diffuse (default)
    if ((materialParams.hasTexture & DIFFUSE_TEXTURE_BIT) != 0) {
        fragColor = texture(diffuseTexture, inTexCoord);
        if (pushConstant.discardOnZeroAlpha == 1 && fragColor.a == 0.0) {
            discard;
        }
    } else {
        fragColor = vec4(materialParams.diffuse, 1.0);
    }
    
    // Specular
    if (pushConstant.renderMode == 1) {
        if ((materialParams.hasTexture & SPECULAR_TEXTURE_BIT) != 0) {
            fragColor = texture(specularTexture, inTexCoord);
        } else {
            fragColor = vec4(materialParams.specular, 1.0);
        }
    }

    // Ambient
    if (pushConstant.renderMode == 2) {
        if ((materialParams.hasTexture & AMBIENT_TEXTURE_BIT) != 0) {
            fragColor = texture(ambientTexture, inTexCoord);
        } else {
            fragColor = vec4(materialParams.ambient, 1.0);
        }
    }

    // Emission
    if (pushConstant.renderMode == 3) {
        if ((materialParams.hasTexture & EMISSION_TEXTURE_BIT) != 0) {
            fragColor = texture(emissionTexture, inTexCoord);
        } else {
            fragColor = vec4(materialParams.emission, 1.0);
        }
    }

    // Shininess
    if (pushConstant.renderMode == 4) {
        fragColor = vec4(vec3(1.0 / log(materialParams.shininess)), 1.0);
    }

    // Index of refraction
    if (pushConstant.renderMode == 5) {
        fragColor = vec4(vec3(materialParams.indexOfRefraction), 1.0);
    }

    // Opacity
    if (pushConstant.renderMode == 6) {
        fragColor = vec4(vec3(materialParams.opacity), 1.0);
    }

    // Normal
    if (pushConstant.renderMode == 7) {
        if ((materialParams.hasTexture & NORMAL_TEXTURE_BIT) != 0) {
            mat3 TBN = mat3(normalize(inTangent), normalize(inBinormal), normalize(inNormal));
            vec3 normal = texture(normalTexture, inTexCoord).rgb * 2.0 - 1.0;
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
