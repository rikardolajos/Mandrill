#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "Shader.h"
#include "Swapchain.h"

namespace Mandrill
{
    class RenderPass
    {
    public:
        MANDRILL_API RenderPass(ptr<Device> pDevice, ptr<Swapchain> pSwapchain);
        MANDRILL_API ~RenderPass();

        MANDRILL_API void recreate();

        MANDRILL_API virtual void begin(VkCommandBuffer cmd, glm::vec4 clearColor) = 0;
        MANDRILL_API virtual void end(VkCommandBuffer cmd) = 0;

        MANDRILL_API VkRenderPass getRenderPass() const
        {
            return mRenderPass;
        }

        MANDRILL_API ptr<Swapchain> getSwapchain() const
        {
            return mpSwapchain;
        }

        MANDRILL_API VkSampleCountFlagBits getSampleCount() const
        {
            return mSampleCount;
        }

    protected:
        virtual void createAttachments() = 0;
        virtual void destroyAttachments() = 0;

        virtual void createFramebuffers() = 0;
        virtual void destroyFramebuffers() = 0;

        ptr<Device> mpDevice;
        ptr<Swapchain> mpSwapchain;

        VkRenderPass mRenderPass;

        VkSampleCountFlagBits mSampleCount;
    private:
    };
} // namespace Mandrill
