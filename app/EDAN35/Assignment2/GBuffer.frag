#version 460

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

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

void main() {
    outPosition = vec4(inWorldPos, 1.0);
    outNormal = vec4(inNormal, 1.0);
    outAlbedo = texture(diffuseTexture, inTexCoord);
}
