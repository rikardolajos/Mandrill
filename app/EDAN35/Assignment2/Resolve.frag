#version 460

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D inPosition;
layout(set = 0, binding = 1) uniform sampler2D inNormal;
layout(set = 0, binding = 2) uniform sampler2D inAlbedo;

layout(push_constant) uniform PushConstant {
    uint renderMode;
} pushConstant;

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    vec3 fragPos = texelFetch(inPosition, coord, 0).rgb;
    vec3 normal = texelFetch(inNormal, coord, 0).rgb;
    vec3 albedo = texelFetch(inAlbedo, coord, 0).rgb;

    vec3 lightPos = vec3(0.0, 5.0, 0.0);

    vec3 L = normalize(lightPos - fragPos);
    vec3 N = normalize(normal) * 2.0 - 1.0;

    vec3 diffuse = albedo * max(0.0, dot(N, L));

    fragColor.a = 1.0;
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
