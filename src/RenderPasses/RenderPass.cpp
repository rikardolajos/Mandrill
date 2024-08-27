#include "RenderPass.h"

#include "Error.h"
#include "Log.h"

using namespace Mandrill;

RenderPass::RenderPass(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                       std::vector<std::shared_ptr<Layout>> pLayouts, std::vector<std::shared_ptr<Shader>> pShaders)
    : mpDevice(pDevice), mpSwapchain(pSwapchain), mpLayouts(pLayouts), mpShaders(pShaders), mRenderPass(VK_NULL_HANDLE)
{
    mPipelineLayouts.resize(pLayouts.size());
    mPipelines.resize(pLayouts.size());

    for (int i = 0; i < pLayouts.size(); i++) {
        auto pLayout = pLayouts[i];

        const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = pLayout->getDescriptorSetLayouts();
        const std::vector<VkPushConstantRange>& pushConstantLayout = pLayout->getPushConstantRanges();

        VkPipelineLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
            .pSetLayouts = descriptorSetLayouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(pushConstantLayout.size()),
            .pPushConstantRanges = pushConstantLayout.data(),
        };

        Check::Vk(vkCreatePipelineLayout(mpDevice->getDevice(), &ci, nullptr, &mPipelineLayouts[i]));
    }
}

RenderPass::~RenderPass()
{
    for (auto pipelineLayout : mPipelineLayouts) {
        vkDestroyPipelineLayout(mpDevice->getDevice(), pipelineLayout, nullptr);
    }
}

void RenderPass::recreatePipelines()
{
    Log::debug("Recreating pipelines");
    destroyPipelines();
    createPipelines();
}
