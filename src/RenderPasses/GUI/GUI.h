#pragma once

#include "Common.h"

#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    class GUI : public RenderPass
    {
    public:
        MANDRILL_API GUI(ptr<Device> pDevice, ptr<Swapchain> pSwapchain);
        MANDRILL_API ~GUI();

        MANDRILL_API void begin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void end(VkCommandBuffer cmd) override;

    protected:
        void createAttachments() override;
        void destroyAttachments() override;

        void createFramebuffers() override;
        void destroyFramebuffers() override;

    private:
        void createRenderPass();

        ptr<Image> mColor;
        std::vector<VkFramebuffer> mFramebuffers;
    };
} // namespace Mandrill
