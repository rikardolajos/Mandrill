#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Descriptor.h"
#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    class RayTracer : public RenderPass
    {
    public:
        MANDRILL_API RayTracer(ptr<Device> pDevice, ptr<Swapchain> pSwapchain);
        MANDRILL_API ~RayTracer();

        MANDRILL_API void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void frameEnd(VkCommandBuffer cmd) override;

    protected:
        void createAttachments() override;
        void destroyAttachments() override;

        void createFramebuffers() override;
        void destroyFramebuffers() override;

    private:
        void createRenderPass();
        void createShaderBindingTable();

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> mpShaderGroups;

        ptr<Buffer> mpShaderBindingTableBuffer;
        ptr<Descriptor> mpAttachmentDescriptor;
    };
} // namespace Mandrill
