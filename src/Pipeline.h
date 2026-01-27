#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "Pass.h"
#include "Scene.h"
#include "Shader.h"

namespace Mandrill
{
    static std::vector<VkVertexInputBindingDescription> defaultBindingDescriptions = {{{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }}};

    static std::vector<VkVertexInputAttributeDescription> defaultAttributeDescriptions = {{
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position),
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal),
        },
        {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, texcoord),
        },
        {
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, tangent),
        },
        {
            .location = 4,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, binormal),
        },
    }};

    /// <summary>
    /// Configuration structure for creating a pipeline. This stucture will default to common settings if not specified.
    /// </summary>
    struct PipelineDesc {

        // Vertex input state //

        /// <summary>
        /// Vertex binding descriptions. Defaults to binding description that fits the Scene abstraction.
        /// </summary>
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;

        /// <summary>
        /// Vertex attribute descriptions. Defaults to attribute description that fits the Scene abstraction.
        /// </summary>
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;


        // Input assembly state //

        /// <summary>
        /// Primitive topology. Defaults to triangle list.
        /// </summary>
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        /// <summary>
        /// Primitive restart enable. Defaults to disabled.
        /// </summary>
        VkBool32 primitiveRestartEnable = VK_FALSE;


        // Rasterization state //

        /// <summary>
        /// Depth clamp enable. Defaults to disabled.
        /// </summary>
        VkBool32 depthClampEnable = VK_FALSE;

        /// <summary>
        /// Rasterizer discard enable. Defaults to disabled.
        /// </summary>
        VkBool32 rasterizerDiscardEnable = VK_FALSE;

        /// <summary>
        /// Polygon mode. Defaults to fill.
        /// </summary>
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;

        /// <summary>
        /// Depth bias enable. Defaults to disabled.
        /// </summary>
        VkBool32 depthBiasEnable = VK_FALSE;

        /// <summary>
        /// Depth bias constant factor. Defaults to 0.0f.
        /// </summary>
        float depthBiasConstantFactor = 0.0f;

        /// <summary>
        /// Depth bias clamp. Defaults to 0.0f.
        /// </summary>
        float depthBiasClamp = 0.0f;

        /// <summary>
        /// Depth biase slope factor. Defaults to 1.0f.
        /// </summary>
        float depthBiasSlopeFactor = 1.0f;


        // Multisample state (samples are set in Pass) //

        /// <summary>
        /// Sample shading enable. Defaults to disabled.
        /// </summary>
        VkBool32 sampleShadingEnable = VK_FALSE;

        /// <summary>
        /// Min sample shading. Defaults to 1.0f.
        /// </summary>
        float minSampleShading = 1.0f;

        /// <summary>
        /// Sample mask pointer. Defaults to nullptr.
        /// </summary>
        VkSampleMask* pSampleMask = nullptr;

        /// <summary>
        /// Alpha to coverage enable. Defaults to disabled.
        /// </summary>
        VkBool32 alphaToCoverageEnable = VK_FALSE;

        /// <summary>
        /// Alpha to one enable. Defaults to disabled.
        /// </summary>
        VkBool32 alphaToOneEnableEnable = VK_FALSE;


        // Color blend attachment state //

        /// <summary>
        /// Blend enable. Defaults to disabled.
        /// </summary>
        VkBool32 blendEnable = VK_FALSE;

        /// <summary>
        /// Source color blend factor. Defaults to SRC_ALPHA.
        /// </summary>
        VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;

        /// <summary>
        /// Destination color blend factor. Defaults to ONE_MINUS_SRC_ALPHA.
        /// </summary>
        VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        /// <summary>
        /// Color blend operation. Defaults to ADD.
        /// </summary>
        VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;

        /// <summary>
        /// Source alpha blend factor. Defaults to ONE.
        /// </summary>
        VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

        /// <summary>
        /// Destination alpha blend factor. Defaults to ZERO.
        /// </summary>
        VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

        /// <summary>
        /// Alpha blend operation. Defaults to ADD.
        /// </summary>
        VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;

        /// <summary>
        /// Color write mask. Defaults to write all components (RGBA).
        /// </summary>
        VkColorComponentFlags colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;


        // Depth-stencil state //

        /// <summary>
        /// Depth test enable. Defaults to enabled.
        /// </summary>
        VkBool32 depthTestEnable = VK_TRUE;

        /// <summary>
        /// Depth write enable. Defaults to enabled.
        /// </summary>
        VkBool32 depthWriteEnable = VK_TRUE;

        /// <summary>
        /// Depth compare operation. Defaults to LESS.
        /// </summary>
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;

        /// <summary>
        /// Depth bounds test enable. Defaults to disabled.
        /// </summary>
        VkBool32 depthBoundsTestEnable = VK_FALSE;

        /// <summary>
        /// Stencil test enable. Defaults to disabled.
        /// </summary>
        VkBool32 stencilTestEnable = VK_FALSE;

        /// <summary>
        /// Min depth bounds. Defaults to 0.0f.
        /// </summary>
        float minDepthBounds = 0.0f;

        /// <summary>
        /// Max depth bounds. Defaults to 1.0f.
        /// </summary>
        float maxDepthBounds = 1.0f;


        // Color blend state //

        /// <summary>
        /// Logic op enable. Defaults to disabled.
        /// </summary>
        VkBool32 logicOpEnable = VK_FALSE;

        /// <summary>
        /// Logic operation. Defaults to COPY.
        /// </summary>
        VkLogicOp logicOp = VK_LOGIC_OP_COPY;

        /// <summary>
        /// Constructor for pipeline description.
        /// </summary>
        /// <param name="bindingDescriptions">Vertex binding description, will default to binding description that fits
        /// the Scene abstraction</param> <param name="attributeDescriptions">Vertex attribute description, will default
        /// to binding description that fits the Scene abstraction</param>
        MANDRILL_API
        PipelineDesc(
            std::vector<VkVertexInputBindingDescription> bindingDescriptions = defaultBindingDescriptions,
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions = defaultAttributeDescriptions)
            : bindingDescriptions(bindingDescriptions), attributeDescriptions(attributeDescriptions)
        {
        }
    };

    /// <summary>
    /// Pipeline class for managing Vulkan graphics pipelines.
    /// </summary>
    class Pipeline
    {
    public:
        MANDRILL_NON_COPYABLE(Pipeline)

        /// <summary>
        /// Create a new pipeline.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="pPass">Pass to use</param>
        /// <param name="pShader">Shader to use</param>
        /// <param name="desc">Description of pipeline</param>
        MANDRILL_API Pipeline(ptr<Device> pDevice, ptr<Pass> pPass, ptr<Shader> pShader,
                              const PipelineDesc& desc = PipelineDesc());

        /// <summary>
        /// Destructor for pipeline.
        /// </summary>
        MANDRILL_API ~Pipeline();

        /// <summary>
        /// Bind a pipeline for rendering and set its dynamic states.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        MANDRILL_API void bind(VkCommandBuffer cmd);

        /// <summary>
        /// Recreate a pipeline. Call this if shader source code has changed and should be reloaded.
        /// </summary>
        MANDRILL_API void recreate();

        /// <summary>
        /// Get the pipeline handle.
        /// </summary>
        /// <returns>Pipeline handle</returns>
        MANDRILL_API VkPipeline getPipeline() const
        {
            return mPipeline;
        }

        /// <summary>
        /// Get the shader module of the pipeline.
        /// </summary>
        /// <returns>Pipeline's shader</returns>
        MANDRILL_API ptr<Shader> getShader() const
        {
            return mpShader;
        }

        /// <summary>
        /// Get the pipeline layout handle.
        /// </summary>
        /// <returns>Pipeline layout handle</returns>
        MANDRILL_API VkPipelineLayout getLayout() const
        {
            return mpShader->getPipelineLayout();
        }

        /// <summary>
        /// Set the cull mode.
        /// </summary>
        /// <param name="cullMode">New cull mode</param>
        MANDRILL_API void setCullMode(VkCullModeFlagBits cullMode)
        {
            mCullMode = cullMode;
        }

        /// <summary>
        /// Set the front face.
        /// </summary>
        /// <param name="frontFace">New front face</param>
        MANDRILL_API void setFrontFace(VkFrontFace frontFace)
        {
            mFrontFace = frontFace;
        }

        /// <summary>
        /// Set line width.
        /// </summary>
        /// <param name="lineWidth">New line width</param>
        MANDRILL_API void setLineWidth(float lineWidth)
        {
            mLineWidth = lineWidth;
        }

    protected:
        /// <summary>
        /// Create the pipeline object.
        /// </summary>
        virtual void createPipeline();

        /// <summary>
        /// Destroy the pipeline object.
        /// </summary>
        virtual void destroyPipeline();

        /// <summary>
        /// Current device.
        /// </summary>
        ptr<Device> mpDevice;

        /// <summary>
        /// Pass used by the pipeline.
        /// </summary>
        ptr<Pass> mpPass;

        /// <summary>
        /// Shader used by the pipeline.
        /// </summary>
        ptr<Shader> mpShader;

        /// <summary>
        /// Vulkan pipeline handle.
        /// </summary>
        VkPipeline mPipeline;

    private:
        PipelineDesc mDesc;

        // Dynamic states
        VkCullModeFlagBits mCullMode = VK_CULL_MODE_NONE;
        VkFrontFace mFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        float mLineWidth = 1.0f;
    };
} // namespace Mandrill
