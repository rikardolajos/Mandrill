#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "Shader.h"
#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    class Pipeline
    {
    public:
        MANDRILL_API Pipeline(ptr<Device> pDevice, ptr<Shader> pShader, ptr<Layout> pLayout, ptr<RenderPass> pRenderPass);
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

    private:
        void createPipeline();

        ptr<Device> mpDevice;
        ptr<RenderPass> mpRenderPass;

        ptr<Shader> mpShader;
        ptr<Layout> mpLayout;

        VkPipeline mPipeline;
        VkPipelineLayout mPipelineLayout;
    };
} // namespace Mandrill
