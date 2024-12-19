#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "RenderPasses/RenderPass.h"
#include "Scene.h"
#include "Shader.h"

namespace Mandrill
{
    static VkVertexInputBindingDescription defaultBindingDescription = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

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
        VkVertexInputBindingDescription bindingDescription;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        VkPolygonMode polygonMode;
        VkBool32 depthTest;
        uint32_t subpass;
        uint32_t attachmentCount;

        MANDRILL_API
        PipelineDesc(
            VkVertexInputBindingDescription bindingDescription = defaultBindingDescription,
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions = defaultAttributeDescriptions,
            VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL, VkBool32 depthTest = VK_TRUE, uint32_t subpass = 0,
            uint32_t attachmentCount = 1)
            : bindingDescription(bindingDescription), attributeDescriptions(attributeDescriptions),
              polygonMode(polygonMode), depthTest(depthTest), subpass(subpass), attachmentCount(attachmentCount)
        {
        }
    };

    class Pipeline
    {
    public:
        ///// <summary>
        ///// Default constructor so RayTracingPipeline can inherit. Use other constructor for regular usage.
        ///// </summary>
        ///// <returns></returns>
        //MANDRILL_API Pipeline()
        //    : mPipeline(nullptr), mPipelineLayout(nullptr), mPolygonMode(VK_POLYGON_MODE_FILL), mDepthTest(VK_FALSE),
        //      mSubpass(0), mAttachmentCount(1)
        //{
        //}

        MANDRILL_API Pipeline(ptr<Device> pDevice, ptr<Shader> pShader, ptr<Layout> pLayout,
                              ptr<RenderPass> pRenderPass, const PipelineDesc& desc = PipelineDesc());
        MANDRILL_API ~Pipeline();

        MANDRILL_API void bind(VkCommandBuffer cmd);

        MANDRILL_API void recreate();

        MANDRILL_API VkPipeline getPipeline() const
        {
            return mPipeline;
        }

        MANDRILL_API VkPipelineLayout getLayout() const
        {
            return mPipelineLayout;
        }

        MANDRILL_API void setCullMode(VkCullModeFlagBits cullMode)
        {
            mCullMode = cullMode;
        }

        MANDRILL_API void setFrontFace(VkFrontFace frontFace)
        {
            mFrontFace = frontFace;
        }

        MANDRILL_API void setLineWidth(float lineWidth)
        {
            mLineWidth = lineWidth;
        }

    protected:
        void createPipeline();
        void destroyPipeline();

        ptr<Device> mpDevice;
        ptr<RenderPass> mpRenderPass;

        ptr<Shader> mpShader;
        ptr<Layout> mpLayout;

        VkPipeline mPipeline;
        VkPipelineLayout mPipelineLayout;

    private:
        VkPolygonMode mPolygonMode;
        VkBool32 mDepthTest;
        uint32_t mSubpass;
        uint32_t mAttachmentCount;

        VkVertexInputBindingDescription mBindingDescription;
        std::vector<VkVertexInputAttributeDescription> mAttributeDescriptions;
        VkCullModeFlagBits mCullMode = VK_CULL_MODE_NONE;
        VkFrontFace mFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        float mLineWidth = 1.0f;
    };
} // namespace Mandrill
