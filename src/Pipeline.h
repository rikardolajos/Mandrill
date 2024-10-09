#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "RenderPasses/RenderPass.h"
#include "Shader.h"

namespace Mandrill
{
    class Pipeline
    {
    public:
        MANDRILL_API Pipeline(ptr<Device> pDevice, ptr<Shader> pShader, ptr<Layout> pLayout,
                              ptr<RenderPass> pRenderPass);
        MANDRILL_API ~Pipeline();

        MANDRILL_API void bind(VkCommandBuffer cmd);

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

        ptr<Device> mpDevice;
        ptr<RenderPass> mpRenderPass;

        ptr<Shader> mpShader;
        ptr<Layout> mpLayout;

        VkPipeline mPipeline;
        VkPipelineLayout mPipelineLayout;

        VkCullModeFlagBits mCullMode = VK_CULL_MODE_NONE;
        VkFrontFace mFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    };
} // namespace Mandrill
