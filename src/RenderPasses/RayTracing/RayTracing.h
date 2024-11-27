#pragma once

#include "Common.h"

#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    class RayTracing : public RenderPass
    {
    public:
        MANDRILL_API RayTracing(ptr<Device> pDevice, ptr<Swapchain> pSwapchain);
        MANDRILL_API ~RayTracing();

        MANDRILL_API void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void frameEnd(VkCommandBuffer cmd) override;

    protected:
        void createAttachments() override;
        void destroyAttachments() override;

        void createFramebuffers() override;
        void destroyFramebuffers() override;

    private:
        void createRenderPass();

        ptr<Image> mColor;
        ptr<Image> mDepth;
        std::vector<VkFramebuffer> mFramebuffers;
    };
} // namespace Mandrill
