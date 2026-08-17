#version 460

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0, rgba16f) uniform image2D inImage;

layout(push_constant) uniform PushConstant {
    int horizontal;
    float threshold;
} pushConstant;

// Gaussian weights for a 9-tap separable blur
const float weights[5] = float[](0.227027, 0.194594, 0.121621, 0.054054, 0.016216);

// Everything above the threshold, which is how the first of the two blurs extracts the bright parts. A threshold of
// zero leaves the image alone, which is what the second blur wants.
vec3 brightPart(ivec2 coord) {
    vec3 color = imageLoad(inImage, clamp(coord, ivec2(0), imageSize(inImage) - 1)).rgb;
    return max(color - vec3(pushConstant.threshold), vec3(0.0));
}

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    ivec2 direction = pushConstant.horizontal != 0 ? ivec2(1, 0) : ivec2(0, 1);

    vec3 color = weights[0] * brightPart(coord);
    for (int i = 1; i < 5; i++) {
        color += weights[i] * brightPart(coord + i * direction);
        color += weights[i] * brightPart(coord - i * direction);
    }

    fragColor = vec4(color, 1.0);
}
