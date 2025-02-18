#include "RayTracingPipeline.h"

#include "Error.h"
#include "Extension.h"
#include "Helpers.h"

using namespace Mandrill;

RayTracingPipeline::RayTracingPipeline(ptr<Device> pDevice, ptr<Layout> pLayout, ptr<Shader> pShader,
                                       const RayTracingPipelineDesc& desc)
    : Pipeline(pDevice, nullptr, pLayout, pShader), mMaxRecursionDepth(desc.maxRecursionDepth),
      mMissGroupCount(desc.missGroupCount), mHitGroupCount(desc.hitGroupCount), mShaderGroups(desc.shaderGroups),
      mGroupSizeAligned(0)
{
    if (!mpDevice->supportsRayTracing()) {
        Log::error("Trying to create a ray-tracing pipeline for a device that does not support it");
        return;
    }

    createPipeline();
    createShaderBindingTable();
}

void RayTracingPipeline::bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mPipeline);
}

void RayTracingPipeline::write(VkCommandBuffer cmd, VkImage image)
{
    // Transition storage image to format for writing
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,

    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void RayTracingPipeline::read(VkCommandBuffer cmd, VkImage image)
{
    // Transition storage image to format for writing
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkDependencyInfo dependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,

    };

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void RayTracingPipeline::recreate()
{
    destroyPipeline();
    mpShader->reload();
    createPipeline();
}

void RayTracingPipeline::createPipeline()
{
    auto stages = mpShader->getStages();

    VkRayTracingPipelineCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = count(stages),
        .pStages = stages.data(),
        .groupCount = count(mShaderGroups),
        .pGroups = mShaderGroups.data(),
        .maxPipelineRayRecursionDepth = mMaxRecursionDepth,
        .layout = mPipelineLayout,
    };

    vkCreateRayTracingPipelinesKHR(mpDevice->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &mPipeline);
}


void RayTracingPipeline::createShaderBindingTable()
{
    uint32_t groupSize = mpDevice->getProperties().rayTracingPipeline.shaderGroupHandleSize;
    mGroupSizeAligned = static_cast<uint32_t>(
        Helpers::alignTo(groupSize, mpDevice->getProperties().rayTracingPipeline.shaderGroupBaseAlignment));

    // Bytes needed for the shader binding table
    uint32_t sbtSize = count(mShaderGroups) * mGroupSizeAligned;
    auto shaderHandleStorage = std::vector<uint8_t>(sbtSize);

    Check::Vk(vkGetRayTracingShaderGroupHandlesKHR(mpDevice->getDevice(), mPipeline, 0, count(mShaderGroups), sbtSize,
                                                   shaderHandleStorage.data()));

    // Allocate buffer for the shader binding table
    mpShaderBindingTableBuffer =
        make_ptr<Buffer>(mpDevice, sbtSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                             VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Write the handles at the aligned positions
    for (uint32_t i = 0; i < count(mShaderGroups); i++) {
        uint8_t* map = static_cast<uint8_t*>(mpShaderBindingTableBuffer->getHostMap());
        std::memcpy(map + i * mGroupSizeAligned, shaderHandleStorage.data() + i * groupSize, groupSize);
    }
}
