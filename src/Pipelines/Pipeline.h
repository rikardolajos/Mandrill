#pragma once

#include "Common.h"

#include "Device.h"
#include "Shader.h"
#include "Swapchain.h"

namespace Mandrill
{
    struct LayoutCreator {
        uint32_t set;
        uint32_t binding;
        VkDescriptorType type;
        VkShaderStageFlags stage;

        MANDRILL_API LayoutCreator(uint32_t set, uint32_t binding, VkDescriptorType type, VkShaderStageFlagBits stage)
            : set(set), binding(binding), type(type), stage(stage)
        {
        }
    };

    class Pipeline
    {
    public:
        MANDRILL_API Pipeline(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                              std::vector<LayoutCreator>& layout, std::shared_ptr<Shader> pShader);
        MANDRILL_API ~Pipeline();

        MANDRILL_API virtual void frameBegin(VkCommandBuffer cmd, float clearColor[4]) = 0;
        MANDRILL_API virtual void frameEnd(VkCommandBuffer cmd) = 0;

    protected:
        virtual void createPipeline() = 0;
        virtual void destroyPipeline() = 0;
        virtual void recreatePipeline() = 0;

        std::shared_ptr<Device> mpDevice;
        std::shared_ptr<Swapchain> mpSwapchain;

        VkPipeline mPipeline;

        VkPipelineLayout mLayout;
        std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;

        VkRenderPass mRenderPass;

        std::shared_ptr<Shader> mpShader;

    private:
        void createLayout(std::vector<LayoutCreator>& layout);
    };
} // namespace Mandrill