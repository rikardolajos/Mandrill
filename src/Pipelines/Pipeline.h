#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "Shader.h"
#include "Swapchain.h"

namespace Mandrill
{
    class Pipeline
    {
    public:
        MANDRILL_API Pipeline(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                              std::shared_ptr<Layout> pLayout, std::shared_ptr<Shader> pShader);
        MANDRILL_API ~Pipeline();

        MANDRILL_API void recreate();

        MANDRILL_API virtual void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) = 0;
        MANDRILL_API virtual void frameEnd(VkCommandBuffer cmd) = 0;

        MANDRILL_API VkPipelineLayout getPipelineLayout() const
        {
            return mPipelineLayout;
        }

        MANDRILL_API VkRenderPass getRenderPass() const
        {
            return mRenderPass;
        }

    protected:
        virtual void createPipeline() = 0;
        virtual void destroyPipeline() = 0;

        std::shared_ptr<Device> mpDevice;
        std::shared_ptr<Swapchain> mpSwapchain;

        VkPipeline mPipeline;
        VkPipelineLayout mPipelineLayout;
        VkRenderPass mRenderPass;
        std::shared_ptr<Layout> mpLayout;
        std::shared_ptr<Shader> mpShader;

    private:
    };
} // namespace Mandrill