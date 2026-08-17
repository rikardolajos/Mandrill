#version 460

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0, rgba16f) uniform image2D inPosition;
layout(set = 0, binding = 1, rgba16f) uniform image2D inNormal;
layout(set = 0, binding = 2, rgba8) uniform image2D inAlbedo;

layout(push_constant) uniform PushConstant {
    float time;
    float lightIntensity;
} pushConstant;

const int LIGHT_COUNT = 3;

const vec3 lightColors[LIGHT_COUNT] = vec3[](
    vec3(1.0, 0.85, 0.6),
    vec3(0.4, 0.7, 1.0),
    vec3(1.0, 0.4, 0.3)
);

vec3 lightPosition(int index, float t) {
    float phase = t + 2.1 * float(index);
    return vec3(
        7.0 * sin(phase / 3.0),
        2.5 * sin(phase / 5.0) + 3.0,
        3.0 * cos(phase / 4.0)
    );
}

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);

    vec3 fragPos = imageLoad(inPosition, coord).rgb;
    vec3 albedo = imageLoad(inAlbedo, coord).rgb;
    vec3 N = normalize(imageLoad(inNormal, coord).rgb);

    // Ambient term, so that surfaces no light reaches are not completely black
    vec3 color = 0.03 * albedo;

    for (int i = 0; i < LIGHT_COUNT; i++) {
        vec3 lightPos = lightPosition(i, pushConstant.time);
        vec3 toLight = lightPos - fragPos;
        float d = length(toLight);
        vec3 L = toLight / max(d, 0.001);

        // Surfaces close to a light blow well past 1.0, which is what gives the bloom passes something to pick up
        color += pushConstant.lightIntensity * lightColors[i] * albedo * max(0.0, dot(N, L)) / (d * d);
    }

    fragColor = vec4(color, 1.0);
}
