#version 460

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inPosition;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inNormal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inAlbedo;

layout(push_constant) uniform PushConstant {
    uint renderMode;
} pushConstant;

void main() {
    vec3 fragPos = subpassLoad(inPosition).rgb;
    vec3 normal = subpassLoad(inNormal).rgb;
    vec3 albedo = subpassLoad(inAlbedo).rgb;

    vec3 lightPos = vec3(0.0, 5.0, 0.0);

    vec3 L = normalize(lightPos - fragPos);
    vec3 N = normalize(normal) * 2.0 - 1.0;

    vec3 diffuse = albedo * max(0.0, dot(N, L));

    if (pushConstant.renderMode == 1) {
        fragColor.rgb = fragPos;
    } else if (pushConstant.renderMode == 2) {
        fragColor.rgb = N * 0.5 + 0.5;
    } else if (pushConstant.renderMode == 3) {
        fragColor.rgb = albedo;
    } else {
        fragColor = vec4(0.05 * albedo + diffuse, 1.0);
    }
}
