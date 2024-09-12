#pragma once

#include "Common.h"

#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    class Rasterizer : public RenderPass
    {
    public:
        MANDRILL_API Rasterizer(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                                const RenderPassDesc& desc);
        MANDRILL_API ~Rasterizer();

        MANDRILL_API void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void frameEnd(VkCommandBuffer cmd) override;

    protected:
        void createPipelines() override;
        void destroyPipelines() override;

        void createAttachments() override;
        void destroyAttachments() override;

        void createFramebuffers() override;
        void destroyFramebuffers() override;

    private:
        void createRenderPass();

        std::unique_ptr<Image> mColor;
        std::unique_ptr<Image> mDepth;
        std::vector<VkFramebuffer> mFramebuffers;
    };
} // namespace Mandrill