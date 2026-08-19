#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "Shader.h"

namespace Mandrill
{
    /// <summary>
    /// Device addresses of the buffers that hold the network, matching the block a shader declares for them:
    ///
    /// <code>
    /// layout(set = 0, binding = 0, scalar) readonly buffer MLPLocations {
    ///     uvec2 params;
    ///     uvec2 weightOffsets;
    ///     uvec2 biasOffsets;
    /// } locMlp;
    /// </code>
    /// </summary>
    struct MLPLocations {
        /// <summary> Weights and biases of every layer, in inferencing-optimal layout </summary>
        VkDeviceAddress params;
        /// <summary> Offset into params of each layer's weight matrix </summary>
        VkDeviceAddress weightOffsets;
        /// <summary> Offset into params of each layer's bias vector </summary>
        VkDeviceAddress biasOffsets;
    };

    /// <summary>
    /// A multi-layer perceptron that is evaluated in a shader with cooperative vector instructions
    /// (VK_NV_cooperative_vector).
    ///
    /// The class only deals with the network itself: it reads the weights and biases from a file, converts them to
    /// the half-precision inferencing-optimal layout the hardware wants, and puts them in device buffers. What the
    /// inputs and the outputs of the network mean, and whether the inputs are encoded in some way before they reach
    /// it, is entirely up to the shader that runs the inference. See app/NeuralNetwork for an example, along with the
    /// GLSL side of this class in app/NeuralNetwork/MLP.glsl.
    ///
    /// The network is described to the shader through specialization constants, so the shader can be written once and
    /// compiled for whichever network was loaded. Pass getSpecializationInfo() when creating the shader.
    ///
    /// The file format is a flat binary that app/NeuralNetwork/train.py writes:
    ///
    /// <code>
    /// uint32                    version, kFileVersion
    /// uint32                    layerCount
    /// for each layer:
    ///     uint32                outputs, rows of the weight matrix
    ///     uint32                inputs, columns of the weight matrix
    ///     float[outputs*inputs] weights, row major
    ///     uint32                biasCount, equal to outputs
    ///     float[biasCount]      biases
    /// </code>
    ///
    /// This is the order PyTorch enumerates the parameters of a Sequential of Linear layers in, so writing it out is
    /// a matter of dumping named_parameters() as they come.
    /// </summary>
    class MLP
    {
    public:
        MANDRILL_NON_COPYABLE(MLP)

        /// <summary> Version of the file format that this class reads </summary>
        static constexpr uint32_t kFileVersion = 1;

        /// <summary> How many specialization constants the network is described to the shader through </summary>
        static constexpr uint32_t kSpecializationConstantCount = 4;

        /// <summary>
        /// Load a network from file. Check isValid() before using it, a network that failed to load leaves the object
        /// empty rather than throwing.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="path">Path to the network file</param>
        MANDRILL_API MLP(ptr<Device> pDevice, const std::filesystem::path& path);

        /// <summary>
        /// Destructor for MLP.
        /// </summary>
        MANDRILL_API ~MLP();

        /// <summary>
        /// Attach the network to the shader that runs the inference. Which set and binding it ends up in is taken
        /// from the shader, so only the name has to match.
        /// </summary>
        /// <param name="pShader">Shader to attach to</param>
        /// <param name="name">Name the shader gives the block holding the buffer addresses</param>
        MANDRILL_API void attachTo(ptr<Shader> pShader, const std::string& name = "locMlp");

        /// <summary>
        /// Bind the set the network lives in. Call after attachTo(). Applications that bind every set at once with
        /// Shader::bindResources() do not need this.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="bindPoint">Bind point in pipeline</param>
        MANDRILL_API void bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint);

        /// <summary>
        /// Check whether a network was loaded.
        /// </summary>
        /// <returns>True if the network is ready to be used</returns>
        MANDRILL_API bool isValid() const
        {
            return mLayerCount > 0;
        }

        /// <summary>
        /// Get the path the network was loaded from.
        /// </summary>
        /// <returns>Path to the network file</returns>
        MANDRILL_API std::filesystem::path getPath() const
        {
            return mPath;
        }

        /// <summary>
        /// Get how many values the network takes as input.
        /// </summary>
        /// <returns>Width of the input layer</returns>
        MANDRILL_API uint32_t getInputCount() const
        {
            return mInputCount;
        }

        /// <summary>
        /// Get how many values the network produces.
        /// </summary>
        /// <returns>Width of the output layer</returns>
        MANDRILL_API uint32_t getOutputCount() const
        {
            return mOutputCount;
        }

        /// <summary>
        /// Get how many layers the network has, counting the input and the output layer.
        /// </summary>
        /// <returns>Number of layers</returns>
        MANDRILL_API uint32_t getLayerCount() const
        {
            return mLayerCount;
        }

        /// <summary>
        /// Get how many neurons the first layer produces. For the plain stack that the shader's mlpForward() runs,
        /// where every hidden layer is the same width, this is the width of all of them, which is what lets the
        /// shader run them in a loop. A network shaped differently loads with a warning and this is only its first
        /// layer's output width.
        /// </summary>
        /// <returns>Width of a hidden layer</returns>
        MANDRILL_API uint32_t getHiddenWidth() const
        {
            return mHiddenWidth;
        }

        /// <summary>
        /// Get the specialization constants that describe the network to the shader, as constant IDs 0 to 3. They
        /// are, in order, input count, output count, layer count and hidden width, which is what
        /// app/NeuralNetwork/MLP.glsl declares.
        ///
        /// This covers a shader whose only specialization constants are the ones describing the network. A shader
        /// that has constants of its own needs all of them in the one VkSpecializationInfo it is created with, so
        /// build that with appendSpecializationConstants() instead.
        ///
        /// The returned pointer stays valid for the lifetime of the MLP.
        /// </summary>
        /// <returns>Specialization info to pass to ShaderDesc, or nullptr if no network was loaded</returns>
        MANDRILL_API const VkSpecializationInfo* getSpecializationInfo() const
        {
            return isValid() ? &mSpecializationInfo : nullptr;
        }

        /// <summary>
        /// Add the constants that describe the network to a set of specialization constants that is being built up,
        /// so that an application with constants of its own can put all of them in the one VkSpecializationInfo a
        /// shader takes.
        ///
        /// The network takes kSpecializationConstantCount consecutive IDs starting at firstConstantId. The shader has
        /// to declare them at the same IDs, which is what MLP_FIRST_CONSTANT_ID in MLP.glsl is for: define it to the
        /// same value before including that file.
        ///
        /// Every specialization constant is one 32-bit word, so an application's own constants go in the same vector,
        /// bit cast if they are floats.
        /// </summary>
        /// <param name="mapEntries">Map entries to append to, to be pointed at by pMapEntries</param>
        /// <param name="values">Constant values to append to, to be pointed at by pData</param>
        /// <param name="firstConstantId">ID the first of the network's constants gets</param>
        MANDRILL_API void appendSpecializationConstants(std::vector<VkSpecializationMapEntry>& mapEntries,
                                                        std::vector<uint32_t>& values, uint32_t firstConstantId) const;

    private:
        // One layer as it was read from file, before it is converted and packed into the parameter buffer
        struct Layer {
            uint32_t inputs;
            uint32_t outputs;
            std::vector<float> weights;
            std::vector<float> biases;
        };

        bool readFile(const std::filesystem::path& path, std::vector<Layer>& layers);
        void warnUnlessPlainStack(const std::filesystem::path& path, const std::vector<Layer>& layers) const;
        void createBuffers(const std::vector<Layer>& layers);
        void setupSpecializationConstants();

        void setupCooperativeVector(bool printDebug = false);
        size_t coopVecQuerySize(uint32_t rows, uint32_t columns) const;
        void coopVecConvert(void* pDst, size_t dstSize, const void* pSrc, uint32_t rows, uint32_t columns) const;

        ptr<Device> mpDevice;

        std::filesystem::path mPath;

        uint32_t mInputCount = 0;
        uint32_t mOutputCount = 0;
        uint32_t mLayerCount = 0;
        uint32_t mHiddenWidth = 0;

        PFN_vkConvertCooperativeVectorMatrixNV mpfnVkConvertCooperativeVectorMatrixNV = nullptr;

        ptr<Buffer> mpParams;
        ptr<Buffer> mpWeightOffsets;
        ptr<Buffer> mpBiasOffsets;

        // Addresses of the three buffers above, and the shader they are attached to
        ptr<Buffer> mpLocations;
        ptr<Shader> mpShader;
        std::string mResourceName;

        // Kept alive because the specialization info points into them
        std::array<uint32_t, kSpecializationConstantCount> mSpecializationConstants = {};
        std::array<VkSpecializationMapEntry, kSpecializationConstantCount> mSpecializationMapEntries = {};
        VkSpecializationInfo mSpecializationInfo = {};
    };
} // namespace Mandrill
