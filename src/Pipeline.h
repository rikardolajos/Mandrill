#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "RenderPasses/RenderPass.h"
#include "Shader.h"

namespace Mandrill
{
    struct PipelineDesc {
        VkPolygonMode polygonMode;
        VkBool32 depthTest;
        uint32_t subpass;
        uint32_t attachmentCount;

        MANDRILL_API PipelineDesc(VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL, VkBool32 depthTest = VK_TRUE,
                     uint32_t subpass = 0, uint32_t attachmentCount = 1)
            : polygonMode(polygonMode), depthTest(depthTest), subpass(subpass), attachmentCount(attachmentCount)
        {
        }
    };

    class Pipeline
    {
    public:
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

    private:
        void createPipeline();
        void destroyPipeline();

        ptr<Device> mpDevice;
        ptr<RenderPass> mpRenderPass;

        ptr<Shader> mpShader;
        ptr<Layout> mpLayout;

        VkPipeline mPipeline;
        VkPipelineLayout mPipelineLayout;

        VkPolygonMode mPolygonMode;
        VkBool32 mDepthTest;
        uint32_t mSubpass;
        uint32_t mAttachmentCount;

        VkCullModeFlagBits mCullMode = VK_CULL_MODE_NONE;
        VkFrontFace mFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    };
} // namespace Mandrill
