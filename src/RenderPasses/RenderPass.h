#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "Shader.h"
#include "Swapchain.h"

namespace Mandrill
{
    struct RenderPassDesc {
        std::vector<ptr<Shader>> shaders;
        std::vector<ptr<Layout>> layouts;

        MANDRILL_API RenderPassDesc(std::vector<ptr<Shader>> shaders, std::vector<ptr<Layout>> layouts)
            : shaders(shaders), layouts(layouts)
        {
        }

        MANDRILL_API RenderPassDesc(ptr<Shader> shader, ptr<Layout> layout)
        {
            shaders.push_back(shader);
            layouts.push_back(layout);
        }
    };

    class RenderPass
    {
    public:
        MANDRILL_API RenderPass(ptr<Device> pDevice, ptr<Swapchain> pSwapchain, const RenderPassDesc& desc);
        MANDRILL_API ~RenderPass();

        MANDRILL_API void recreatePipelines();

        MANDRILL_API virtual void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) = 0;
        MANDRILL_API virtual void frameEnd(VkCommandBuffer cmd) = 0;

        MANDRILL_API VkRenderPass getRenderPass() const
        {
            return mRenderPass;
        }

        MANDRILL_API VkPipelineLayout getPipelineLayout(uint32_t index) const
        {
            return mPipelineLayouts[index];
        }

    protected:
        virtual void createPipelines() = 0;
        virtual void destroyPipelines() = 0;

        virtual void createAttachments() = 0;
        virtual void destroyAttachments() = 0;

        virtual void createFramebuffers() = 0;
        virtual void destroyFramebuffers() = 0;

        ptr<Device> mpDevice;
        ptr<Swapchain> mpSwapchain;

        VkRenderPass mRenderPass;

        std::vector<VkPipeline> mPipelines;
        std::vector<VkPipelineLayout> mPipelineLayouts;

        std::vector<ptr<Layout>> mpLayouts;
        std::vector<ptr<Shader>> mpShaders;

    private:
    };
} // namespace Mandrill
