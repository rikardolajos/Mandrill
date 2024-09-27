#pragma once

#include "Common.h"

#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    class RayTracer : public RenderPass
    {
    public:
        MANDRILL_API RayTracer(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                               const RenderPassDesc& desc);
        MANDRILL_API ~RayTracer();

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
        void createShaderBindingTable();

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> mpShaderGroups;

        std::shared_ptr<Buffer> mpShaderBindingTableBuffer;
        std::shared_ptr<Descriptor> mpAttachmentDescriptor;
    };
} // namespace Mandrill
