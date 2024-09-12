#pragma once

#include "Common.h"

#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    enum DeferredInputAttachment {
        DEFERRED_INPUT_ATTACHMENT_POSITION,
        DEFERRED_INPUT_ATTACHMENT_NORMAL,
        DEFERRED_INPUT_ATTACHMENT_ALBEDO,
    };

    class Deferred : public RenderPass
    {
    public:
        MANDRILL_API Deferred(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                              const RenderPassDesc& desc);

        MANDRILL_API ~Deferred();

        MANDRILL_API void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void frameEnd(VkCommandBuffer cmd) override;

        MANDRILL_API void nextSubpass(VkCommandBuffer cmd);

        MANDRILL_API std::shared_ptr<Image> getInputAttachmentInfo(DeferredInputAttachment attachment)
        {
            switch (attachment) {
            case DEFERRED_INPUT_ATTACHMENT_POSITION:
                return mpPosition;
            case DEFERRED_INPUT_ATTACHMENT_NORMAL:
                return mpNormal;
            case DEFERRED_INPUT_ATTACHMENT_ALBEDO:
                return mpAlbedo;
            default:
                return nullptr;
            }
        }

    protected:
        void createPipelines() override;
        void destroyPipelines() override;

        void createAttachments() override;
        void destroyAttachments() override;

        void createFramebuffers() override;
        void destroyFramebuffers() override;

    private:
        void createRenderPass();

        std::shared_ptr<Image> mpPosition;
        std::shared_ptr<Image> mpNormal;
        std::shared_ptr<Image> mpAlbedo;
        std::shared_ptr<Image> mpDepth;
        std::vector<VkFramebuffer> mFramebuffers;
    };
} // namespace Mandrill
