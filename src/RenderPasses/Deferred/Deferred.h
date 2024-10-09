#pragma once

#include "Common.h"

#include "Descriptor.h"
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
        MANDRILL_API Deferred(ptr<Device> pDevice, ptr<Swapchain> pSwapchain);

        MANDRILL_API ~Deferred();

        MANDRILL_API void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void frameEnd(VkCommandBuffer cmd) override;

        MANDRILL_API void nextSubpass(VkCommandBuffer cmd);

    protected:
        void createAttachments() override;
        void destroyAttachments() override;

        void createFramebuffers() override;
        void destroyFramebuffers() override;

    private:
        void createRenderPass();

        ptr<Image> mpPosition;
        ptr<Image> mpNormal;
        ptr<Image> mpAlbedo;
        ptr<Image> mpDepth;
        std::vector<VkFramebuffer> mFramebuffers;

        ptr<Descriptor> mpInputAttachmentDescriptor;
    };
} // namespace Mandrill
