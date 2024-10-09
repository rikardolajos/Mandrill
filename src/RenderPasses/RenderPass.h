#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "Shader.h"
#include "Swapchain.h"

namespace Mandrill
{
    //struct RenderPassDesc {
    //    std::vector<ptr<Shader>> shaders;
    //    std::vector<ptr<Layout>> layouts;

    //    MANDRILL_API RenderPassDesc(std::vector<ptr<Shader>> shaders, std::vector<ptr<Layout>> layouts)
    //        : shaders(shaders), layouts(layouts)
    //    {
    //    }

    //    MANDRILL_API RenderPassDesc(ptr<Shader> shader, ptr<Layout> layout)
    //    {
    //        shaders.push_back(shader);
    //        layouts.push_back(layout);
    //    }
    //};

    class RenderPass
    {
    public:
        MANDRILL_API RenderPass(ptr<Device> pDevice, ptr<Swapchain> pSwapchain);
        MANDRILL_API ~RenderPass();

        MANDRILL_API void recreate();

        MANDRILL_API virtual void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) = 0;
        MANDRILL_API virtual void frameEnd(VkCommandBuffer cmd) = 0;

        MANDRILL_API VkRenderPass getRenderPass() const
        {
            return mRenderPass;
        }

        MANDRILL_API ptr<Swapchain> getSwapchain() const
        {
            return mpSwapchain;
        }

    protected:
        virtual void createAttachments() = 0;
        virtual void destroyAttachments() = 0;

        virtual void createFramebuffers() = 0;
        virtual void destroyFramebuffers() = 0;

        ptr<Device> mpDevice;
        ptr<Swapchain> mpSwapchain;

        VkRenderPass mRenderPass;

    private:
    };
} // namespace Mandrill
