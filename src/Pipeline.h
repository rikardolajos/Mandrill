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

    struct PipelineDesc {
        // Vertex input state
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        // Input assembly state
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkBool32 primitiveRestartEnable = VK_FALSE;
        // Rasterization state
        VkBool32 depthClampEnable = VK_FALSE;
        VkBool32 rasterizerDiscardEnable = VK_FALSE;
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        VkBool32 depthBiasEnable = VK_FALSE;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlopeFactor = 1.0f;
        // Multisample state (samples are set in Pass)
        VkBool32 sampleShadingEnable = VK_FALSE;
        float minSampleShading = 1.0f;
        VkSampleMask* pSampleMask = nullptr;
        VkBool32 alphaToCoverageEnable = VK_FALSE;
        VkBool32 alphaToOneEnableEnable = VK_FALSE;
        // Color blend attachment state
        VkBool32 blendEnable = VK_FALSE;
        VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
        VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
        VkColorComponentFlags colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // Depth-stencil state
        VkBool32 depthTestEnable = VK_TRUE;
        VkBool32 depthWriteEnable = VK_TRUE;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
        VkBool32 depthBoundsTestEnable = VK_FALSE;
        VkBool32 stencilTestEnable = VK_FALSE;
        float minDepthBounds = 0.0f;
        float maxDepthBounds = 1.0f;
        // Color blend state
        VkBool32 logicOpEnable = VK_FALSE;
        VkLogicOp logicOp = VK_LOGIC_OP_COPY;

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
        /// <returns></returns>
        MANDRILL_API void bind(VkCommandBuffer cmd);

        /// <summary>
        /// Recreate a pipeline. Call this if shader source code has changed and should be reloaded.
        /// </summary>
        /// <returns></returns>
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
        /// <returns></returns>
        MANDRILL_API void setCullMode(VkCullModeFlagBits cullMode)
        {
            mCullMode = cullMode;
        }

        /// <summary>
        /// Set the front face.
        /// </summary>
        /// <param name="frontFace">New front face</param>
        /// <returns></returns>
        MANDRILL_API void setFrontFace(VkFrontFace frontFace)
        {
            mFrontFace = frontFace;
        }

        /// <summary>
        /// Set line width.
        /// </summary>
        /// <param name="lineWidth">New line width</param>
        /// <returns></returns>
        MANDRILL_API void setLineWidth(float lineWidth)
        {
            mLineWidth = lineWidth;
        }

    protected:
        virtual void createPipeline();
        virtual void destroyPipeline();

        ptr<Device> mpDevice;
        ptr<Pass> mpPass;

        ptr<Shader> mpShader;
        ptr<Layout> mpLayout;

        VkPipeline mPipeline;

    private:
        PipelineDesc mDesc;

        // Dynamic states
        VkCullModeFlagBits mCullMode = VK_CULL_MODE_NONE;
        VkFrontFace mFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        float mLineWidth = 1.0f;
    };
} // namespace Mandrill
