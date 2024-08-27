#pragma once

#include "Common.h"

#include "Device.h"
#include "Layout.h"
#include "Shader.h"
#include "Swapchain.h"

namespace Mandrill
{
    class RenderPass
    {
    public:
        MANDRILL_API RenderPass(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                                std::vector<std::shared_ptr<Layout>> pLayouts,
                                std::vector<std::shared_ptr<Shader>> pShaders);
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

        std::shared_ptr<Device> mpDevice;
        std::shared_ptr<Swapchain> mpSwapchain;

        VkRenderPass mRenderPass;

        std::vector<VkPipeline> mPipelines;
        std::vector<VkPipelineLayout> mPipelineLayouts;

        std::vector<std::shared_ptr<Layout>> mpLayouts;
        std::vector<std::shared_ptr<Shader>> mpShaders;

    private:
    };
} // namespace Mandrill
