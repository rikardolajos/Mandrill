#ifndef MLP_GLSL
#define MLP_GLSL

/*
 * Inference of a multi-layer perceptron with cooperative vector instructions, the shader side of Mandrill's MLP
 * class. The network itself is loaded, converted and bound by MLP (src/MLP.h), all this file does is run it.
 *
 * The network is described through specialization constants that the MLP class fills in from the file it loaded, so
 * this code does not have to be changed when the network does. Create the shader with MLP::getSpecializationInfo().
 *
 * The shader that includes this file decides what the inputs and the outputs mean, and applies whatever encoding the
 * network was trained with. Declare the block holding the network somewhere in the including shader:
 *
 *     layout(set = 0, binding = 0, scalar) readonly buffer MLPLocations {
 *         uvec2 params;
 *         uvec2 weightOffsets;
 *         uvec2 biasOffsets;
 *     } locMlp;
 *
 * and evaluate the network with:
 *
 *     MLP mlp = MLP(locMlp.params, locMlp.weightOffsets, locMlp.biasOffsets);
 *     MLPInput inputVec;
 *     MLPOutput outputVec;
 *     // fill in inputVec
 *     mlpForward(mlp, inputVec, outputVec);
 *
 * Every hidden layer is followed by a ReLU. The output layer is left raw, so that the including shader can apply
 * whichever activation the network was trained with.
 */

#extension GL_NV_cooperative_vector : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require

// Weights, biases and the vectors flowing between the layers are all half precision
#define MLP_COMPONENT_TYPE gl_ComponentTypeFloat16NV

// Description of the loaded network, filled in by MLP::getSpecializationInfo()
layout(constant_id = 0) const int MLP_INPUT_COUNT = 2;
layout(constant_id = 1) const int MLP_OUTPUT_COUNT = 3;
layout(constant_id = 2) const int MLP_LAYER_COUNT = 4;
layout(constant_id = 3) const int MLP_HIDDEN_WIDTH = 64;

// GLSL has no typedef, so the vectors that go in and out of the network are reached through these
#define MLPInput coopvecNV<float16_t, MLP_INPUT_COUNT>
#define MLPOutput coopvecNV<float16_t, MLP_OUTPUT_COUNT>
#define MLPHidden coopvecNV<float16_t, MLP_HIDDEN_WIDTH>

// All the weights and biases of the network, laid out back to back
layout(buffer_reference, scalar) readonly buffer MLPParam {
    float16_t values[];
};

// Where in MLPParam each layer's weight matrix or bias vector begins
layout(buffer_reference, scalar) readonly buffer MLPOffset {
    uint values[];
};

// Addresses of the buffers above, as they are laid out by Mandrill::MLPLocations
struct MLP {
    uvec2 params;
    uvec2 weightOffsets;
    uvec2 biasOffsets;
};

void mlpForward(MLP mlp, in MLPInput inputVec, out MLPOutput outputVec)
{
    MLPParam params = MLPParam(mlp.params);
    MLPOffset weightOffsets = MLPOffset(mlp.weightOffsets);
    MLPOffset biasOffsets = MLPOffset(mlp.biasOffsets);

    MLPHidden hiddenVec;

    /* Input layer */
    coopVecMatMulAddNV(hiddenVec, inputVec, MLP_COMPONENT_TYPE, params.values, weightOffsets.values[0],
                       MLP_COMPONENT_TYPE, params.values, biasOffsets.values[0], MLP_COMPONENT_TYPE, MLP_HIDDEN_WIDTH,
                       MLP_INPUT_COUNT, gl_CooperativeVectorMatrixLayoutInferencingOptimalNV, false, 0);

    hiddenVec = max(MLPHidden(float16_t(0)), hiddenVec);

    /* Hidden layers */
    for (int i = 1; i < MLP_LAYER_COUNT - 1; i++) {
        coopVecMatMulAddNV(hiddenVec, hiddenVec, MLP_COMPONENT_TYPE, params.values, weightOffsets.values[i],
                           MLP_COMPONENT_TYPE, params.values, biasOffsets.values[i], MLP_COMPONENT_TYPE,
                           MLP_HIDDEN_WIDTH, MLP_HIDDEN_WIDTH, gl_CooperativeVectorMatrixLayoutInferencingOptimalNV,
                           false, 0);

        hiddenVec = max(MLPHidden(float16_t(0)), hiddenVec);
    }

    /* Output layer, activation is left to the caller */
    coopVecMatMulAddNV(outputVec, hiddenVec, MLP_COMPONENT_TYPE, params.values,
                       weightOffsets.values[MLP_LAYER_COUNT - 1], MLP_COMPONENT_TYPE, params.values,
                       biasOffsets.values[MLP_LAYER_COUNT - 1], MLP_COMPONENT_TYPE, MLP_OUTPUT_COUNT,
                       MLP_HIDDEN_WIDTH, gl_CooperativeVectorMatrixLayoutInferencingOptimalNV, false, 0);
}

#endif
