#version 460

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0, rgba16f) uniform image2D inPosition;
layout(set = 0, binding = 1, rgba16f) uniform image2D inNormal;
layout(set = 0, binding = 2, rgba8) uniform image2D inAlbedo;

layout(push_constant) uniform PushConstant {
    uint renderMode;
    float time;
} pushConstant;

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    vec3 fragPos = imageLoad(inPosition, coord).rgb;
    vec3 normal = imageLoad(inNormal, coord).rgb;
    vec3 albedo = imageLoad(inAlbedo, coord).rgb;

    vec3 lightPos = vec3(
        7.0 * sin(pushConstant.time / 3.0),
        2.5 * sin(pushConstant.time / 5.0) + 2.5,
        0.0
    );

    float d = length(lightPos - fragPos);
    vec3 L = normalize(lightPos - fragPos);
    vec3 N = normalize(normal);

    vec3 diffuse = 5.0 * albedo * max(0.0, dot(N, L)) / (d * d);

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
