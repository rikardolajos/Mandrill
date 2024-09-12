#include "RenderPass.h"

#include "Error.h"
#include "Log.h"

using namespace Mandrill;

RenderPass::RenderPass(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                       const RenderPassDesc& desc)
    : mpDevice(pDevice), mpSwapchain(pSwapchain), mpLayouts(desc.layouts), mpShaders(desc.shaders), mRenderPass(VK_NULL_HANDLE)
{
    mPipelineLayouts.resize(mpLayouts.size());
    mPipelines.resize(mpLayouts.size());

    for (int i = 0; i < mpLayouts.size(); i++) {
        auto& pLayout = mpLayouts[i];

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
    destroyFramebuffers();
    destroyAttachments();
    createAttachments();
    createFramebuffers();
    createPipelines();
}
