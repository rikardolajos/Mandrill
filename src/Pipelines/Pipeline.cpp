#include "Pipeline.h"

#include "Error.h"
#include "Log.h"

using namespace Mandrill;

Pipeline::Pipeline(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                   std::shared_ptr<Layout> pLayout, std::shared_ptr<Shader> pShader)
    : mpDevice(pDevice), mpSwapchain(pSwapchain), mpLayout(pLayout), mpShader(pShader), mPipeline(VK_NULL_HANDLE),
      mRenderPass(VK_NULL_HANDLE)
{
    const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = mpLayout->getDescriptorSetLayouts();
    const std::vector<VkPushConstantRange>& pushConstantLayout = mpLayout->getPushConstantRanges();

    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
        .pSetLayouts = descriptorSetLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstantLayout.size()),
        .pPushConstantRanges = pushConstantLayout.data(),
    };

    Check::Vk(vkCreatePipelineLayout(mpDevice->getDevice(), &ci, nullptr, &mPipelineLayout));
}

Pipeline::~Pipeline()
{
    vkDestroyPipelineLayout(mpDevice->getDevice(), mPipelineLayout, nullptr);
}

void Pipeline::recreate()
{
    Log::debug("Recreating pipeline");
    destroyPipeline();
    createPipeline();
}