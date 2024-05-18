#version 460

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexture;

void main() {
    fragColor = texture(diffuseTexture, texCoord);
}
