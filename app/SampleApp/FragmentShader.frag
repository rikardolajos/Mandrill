#version 460

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 1) uniform sampler2D diffuseTexture;

void main() {
    fragColor = texture(diffuseTexture, texCoord);
}
