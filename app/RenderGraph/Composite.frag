#version 460

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0, rgba16f) uniform image2D inHdr;
layout(set = 0, binding = 1, rgba16f) uniform image2D inBloom;
layout(set = 0, binding = 2, rgba16f) uniform image2D inPosition;
layout(set = 0, binding = 3, rgba16f) uniform image2D inNormal;
layout(set = 0, binding = 4, rgba8) uniform image2D inAlbedo;

layout(push_constant) uniform PushConstant {
    float exposure;
    float bloomStrength;
    int renderMode;
} pushConstant;

const int RENDER_MODE_FINAL = 0;
const int RENDER_MODE_HDR = 1;
const int RENDER_MODE_BLOOM = 2;
const int RENDER_MODE_POSITION = 3;
const int RENDER_MODE_NORMAL = 4;
const int RENDER_MODE_ALBEDO = 5;

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);

    vec3 hdr = imageLoad(inHdr, coord).rgb;
    vec3 bloom = imageLoad(inBloom, coord).rgb;

    vec3 color;
    switch (pushConstant.renderMode) {
    case RENDER_MODE_HDR:
        color = hdr;
        break;
    case RENDER_MODE_BLOOM:
        color = bloom;
        break;
    case RENDER_MODE_POSITION:
        color = imageLoad(inPosition, coord).rgb;
        break;
    case RENDER_MODE_NORMAL:
        color = imageLoad(inNormal, coord).rgb * 0.5 + 0.5;
        break;
    case RENDER_MODE_ALBEDO:
        color = imageLoad(inAlbedo, coord).rgb;
        break;
    default:
        color = hdr + pushConstant.bloomStrength * bloom;
        break;
    }

    // The modes showing a G-buffer attachment are already in display range, the others are not
    if (pushConstant.renderMode <= RENDER_MODE_BLOOM) {
        color = vec3(1.0) - exp(-color * pushConstant.exposure);
    }

    // Encode for the swapchain, which is a linear format
    fragColor = vec4(pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
}
