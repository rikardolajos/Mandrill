#version 460

#extension GL_GOOGLE_include_directive : require

#include "MLP.glsl"

#define M_PI 3.14159265358979323846

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;

// The network was trained on the image composited over this background, so the reference has to be composited over
// the same one for the two to be comparable. Must match --background in train.py.
const vec3 BACKGROUND = vec3(0.1);

// View modes, must match NeuralNetwork.cpp
const uint MODE_NETWORK = 0;
const uint MODE_REFERENCE = 1;
const uint MODE_SPLIT = 2;
const uint MODE_DIFFERENCE = 3;

// The network takes the coordinate itself plus a sine and a cosine of it per octave, which is what lets a small MLP
// hold on to detail that raw coordinates would smooth away. Must match encode() in train.py.
const int FREQUENCY_COUNT = (MLP_INPUT_COUNT - 2) / 4;

layout(set = 0, binding = 0, scalar) readonly buffer MLPLocations {
    uvec2 params;
    uvec2 weightOffsets;
    uvec2 biasOffsets;
} locMlp;

layout(set = 1, binding = 0) uniform sampler2D referenceImage;

layout(push_constant) uniform PushConstant {
    vec2 resolution;
    vec2 center;
    float zoom;
    float split;
    uint mode;
    float differenceScale;
} pushConstant;

void encode(vec2 p, out MLPInput inputVec)
{
    inputVec[0] = float16_t(p.x);
    inputVec[1] = float16_t(p.y);

    for (int i = 0; i < FREQUENCY_COUNT; i++) {
        vec2 f = exp2(float(i)) * M_PI * p;
        inputVec[2 + 4 * i + 0] = float16_t(sin(f.x));
        inputVec[2 + 4 * i + 1] = float16_t(sin(f.y));
        inputVec[2 + 4 * i + 2] = float16_t(cos(f.x));
        inputVec[2 + 4 * i + 3] = float16_t(cos(f.y));
    }
}

vec3 evaluateNetwork(vec2 p)
{
    MLP mlp = MLP(locMlp.params, locMlp.weightOffsets, locMlp.biasOffsets);

    MLPInput inputVec;
    MLPOutput outputVec;

    encode(p, inputVec);
    mlpForward(mlp, inputVec, outputVec);

    // The network was trained with a sigmoid on its output, so the raw output is turned into a colour here
    vec3 color = vec3(float(outputVec[0]), float(outputVec[1]), float(outputVec[2]));
    return 1.0 / (1.0 + exp(-color));
}

vec3 sampleReference(vec2 p)
{
    if (any(lessThan(p, vec2(0.0))) || any(greaterThan(p, vec2(1.0)))) {
        return BACKGROUND;
    }

    // Mandrill loads textures bottom up, while the network was trained with the first row of the image at v = 0, so
    // the reference has to be flipped to line up with it
    vec4 texel = texture(referenceImage, vec2(p.x, 1.0 - p.y));
    return mix(BACKGROUND, texel.rgb, texel.a);
}

void main()
{
    // Keep the image square whatever the window is, and let the view be panned and zoomed so that what the network
    // does outside the unit square it was trained on can be seen as well
    vec2 p = inTexCoord - 0.5;
    p.x *= pushConstant.resolution.x / pushConstant.resolution.y;
    p = p / pushConstant.zoom + pushConstant.center + 0.5;

    vec3 color;

    switch (pushConstant.mode) {
    case MODE_REFERENCE:
        color = sampleReference(p);
        break;

    case MODE_SPLIT:
        if (inTexCoord.x < pushConstant.split) {
            color = sampleReference(p);
        } else {
            color = evaluateNetwork(p);
        }

        // A thin line where the two meet, so that the split is visible even where they agree
        if (abs(inTexCoord.x - pushConstant.split) < 0.5 / pushConstant.resolution.x) {
            color = vec3(1.0);
        }
        break;

    case MODE_DIFFERENCE:
        color = vec3(pushConstant.differenceScale) * abs(evaluateNetwork(p) - sampleReference(p));
        break;

    default:
        color = evaluateNetwork(p);
        break;
    }

    fragColor = vec4(color, 1.0);
}
