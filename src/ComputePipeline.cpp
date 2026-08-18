#include "ComputePipeline.h"

#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

ComputePipeline::ComputePipeline(ptr<Device> pDevice, ptr<Shader> pShader, const ComputePipelineDesc& desc)
    : Pipeline(pDevice, nullptr, pShader), mFlags(desc.flags), mLocalSize(1)
{
    createPipeline();
}

void ComputePipeline::bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, mPipeline);
}

void ComputePipeline::recreate()
{
    destroyPipeline();
    mpShader->reload();
    createPipeline();
}

void ComputePipeline::dispatch(VkCommandBuffer cmd, uint32_t workItemsX, uint32_t workItemsY, uint32_t workItemsZ)
{
    // Round up so that every work item is covered, which leaves the invocations past the end for the shader to guard
    // against
    uint32_t groupCountX = (workItemsX + mLocalSize.x - 1) / mLocalSize.x;
    uint32_t groupCountY = (workItemsY + mLocalSize.y - 1) / mLocalSize.y;
    uint32_t groupCountZ = (workItemsZ + mLocalSize.z - 1) / mLocalSize.z;

    dispatchGroups(cmd, groupCountX, groupCountY, groupCountZ);
}

void ComputePipeline::dispatchGroups(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY,
                                     uint32_t groupCountZ)
{
    vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
}

void ComputePipeline::write(VkCommandBuffer cmd, VkImage image)
{
    Helpers::imageBarrier(cmd, image, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                          VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

void ComputePipeline::read(VkCommandBuffer cmd, VkImage image)
{
    Helpers::imageBarrier(cmd, image, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

void ComputePipeline::createPipeline()
{
    auto stages = mpShader->getStages();

    if (count(stages) != 1) {
        Log::Error("A compute pipeline takes a shader with a single compute stage, but {} stages were given",
                   count(stages));
        return;
    }

    // The dispatch is given in work items, so the local size has to be known. It is reflected from the shader, and
    // is only available once the shader has been loaded, so it is picked up on every (re)creation.
    mLocalSize = mpShader->getLocalSize();

    VkComputePipelineCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .flags = mFlags,
        .stage = stages[0],
        .layout = mpShader->getPipelineLayout(),
    };

    Check::Vk(vkCreateComputePipelines(mpDevice->getDevice(), VK_NULL_HANDLE, 1, &ci, nullptr, &mPipeline));
}
