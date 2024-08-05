#version 460

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 2) uniform MaterialParams {
    vec3 diffuse;
    float shininess;
    vec3 specular;
    float indexOfRefraction;
    vec3 ambient;
    float opacity;
    vec3 emission;
    uint hasTexture;
} materialParams;

layout(set = 0, binding = 3) uniform sampler2D diffuseTexture;
layout(set = 0, binding = 4) uniform sampler2D specularTexture;
layout(set = 0, binding = 5) uniform sampler2D ambientTexture;
layout(set = 0, binding = 6) uniform sampler2D emissionTexture;
layout(set = 0, binding = 7) uniform sampler2D normalTexture;

layout(push_constant) uniform PushConstant {
    uint renderMode;
    uint discardOnZeroAlpha;
} pushConstant;

const uint DIFFUSE_TEXTURE_BIT  = 1 << 0;
const uint SPECULAR_TEXTURE_BIT = 1 << 1;
const uint AMBIENT_TEXTURE_BIT = 1 << 2;
const uint EMISSION_TEXTURE_BIT = 1 << 3;
const uint NORMAL_TEXTURE_BIT = 1 << 4;

void main() {
    // Diffuse (default)
    if ((materialParams.hasTexture & DIFFUSE_TEXTURE_BIT) != 0) {
        fragColor = texture(diffuseTexture, texCoord);
        if (pushConstant.discardOnZeroAlpha == 1 && fragColor.a == 0.0) {
            discard;
        }
    } else {
        fragColor = vec4(materialParams.diffuse, 1.0);
    }
    
    // Specular
    if (pushConstant.renderMode == 1) {
        if ((materialParams.hasTexture & SPECULAR_TEXTURE_BIT) != 0) {
            fragColor = texture(specularTexture, texCoord);
        } else {
            fragColor = vec4(materialParams.specular, 1.0);
        }
    }

    // Ambient
    if (pushConstant.renderMode == 2) {
        if ((materialParams.hasTexture & AMBIENT_TEXTURE_BIT) != 0) {
            fragColor = texture(ambientTexture, texCoord);
        } else {
            fragColor = vec4(materialParams.ambient, 1.0);
        }
    }

    // Emission
    if (pushConstant.renderMode == 3) {
        if ((materialParams.hasTexture & EMISSION_TEXTURE_BIT) != 0) {
            fragColor = texture(emissionTexture, texCoord);
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
            fragColor = texture(normalTexture, texCoord);
        } else {
            fragColor = vec4(normal, 1.0);
        }
        fragColor.rgb = fragColor.rgb * 0.5 + 0.5;
    }

    // Texture coordinates
    if (pushConstant.renderMode == 8) {
        fragColor = vec4(texCoord, 0.0, 1.0);
    }
}
