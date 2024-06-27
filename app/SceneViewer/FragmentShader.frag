#version 460

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 2) uniform MaterialColorUniforms {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
} materialColor;

layout(set = 1, binding = 3) uniform sampler2D ambientTexture;
layout(set = 1, binding = 4) uniform sampler2D diffuseTexture;
layout(set = 1, binding = 5) uniform sampler2D specularTexture;

void main() {
    fragColor = texture(diffuseTexture, texCoord);
}
