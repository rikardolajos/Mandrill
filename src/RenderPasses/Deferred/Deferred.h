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

        MANDRILL_API void begin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void end(VkCommandBuffer cmd) override;

        MANDRILL_API ptr<Image> getPositionImage() const
        {
            return mpPosition;
        }

        MANDRILL_API ptr<Image> getNormalImage() const
        {
            return mpNormal;
        }

        MANDRILL_API ptr<Image> getAlbedoImage() const
        {
            return mpAlbedo;
        }

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
    };
} // namespace Mandrill
