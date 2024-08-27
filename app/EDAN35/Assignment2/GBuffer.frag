#version 460

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

layout(set = 0, binding = 1) uniform sampler2D diffuseTexture;

void main() {
    outPosition = vec4(inWorldPos, 1.0);
    outNormal = vec4(inNormal, 1.0);
    outAlbedo = texture(diffuseTexture, inTexCoord);
}
