#include "RayTracingPipeline.h"

#include "Error.h"
#include "Extension.h"
#include "Helpers.h"

using namespace Mandrill;

enum ShaderGroupType {
    SHADER_GROUP_RAYGEN,
    SHADER_GROUP_MISS,
    SHADER_GROUP_HIT,
    SHADER_GROUP_COUNT,
};

RayTracingPipeline::RayTracingPipeline(ptr<Device> pDevice, ptr<Shader> pShader, ptr<Layout> pLayout,
                                       const RayTracingPipelineDesc& desc)
    : Pipeline(pDevice, pShader, pLayout, nullptr), mMaxRecursionDepth(desc.maxRecursionDepth),
      mMissGroupCount(desc.missGroupCount), mHitGroupCount(desc.hitGroupCount), mShaderGroups(desc.shaderGroups)
{
    if (!mpDevice->supportsRayTracing()) {
        Log::error("Trying to create a ray-tracing pipeline for a device that does not support it");
        return;
    }

    // const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts = mpLayout->getDescriptorSetLayouts();
    // const std::vector<VkPushConstantRange>& pushConstantLayout = mpLayout->getPushConstantRanges();

    // VkPipelineLayoutCreateInfo ci = {
    //     .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    //     .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
    //     .pSetLayouts = descriptorSetLayouts.data(),
    //     .pushConstantRangeCount = static_cast<uint32_t>(pushConstantLayout.size()),
    //     .pPushConstantRanges = pushConstantLayout.data(),
    // };

    // Check::Vk(vkCreatePipelineLayout(mpDevice->getDevice(), &ci, nullptr, &mPipelineLayout));

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
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
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

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);
}

void RayTracingPipeline::read(VkCommandBuffer cmd, VkImage image)
{
    // Barrier to ensure writes are done before using rendered output as color attachment
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
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

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
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
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(mShaderGroups.size()),
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
    uint32_t sbtSize = static_cast<uint32_t>(mShaderGroups.size()) * mGroupSizeAligned;
    auto shaderHandleStorage = std::vector<uint8_t>(sbtSize);

    Check::Vk(vkGetRayTracingShaderGroupHandlesKHR(mpDevice->getDevice(), mPipeline, 0,
                                                   static_cast<uint32_t>(mShaderGroups.size()), sbtSize,
                                                   shaderHandleStorage.data()));

    // Allocate buffer for the shader binding table
    mpShaderBindingTableBuffer =
        make_ptr<Buffer>(mpDevice, sbtSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                             VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Write the handles at the aligned positions
    for (uint32_t i = 0; i < static_cast<uint32_t>(mShaderGroups.size()); i++) {
        uint8_t* map = static_cast<uint8_t*>(mpShaderBindingTableBuffer->getHostMap());
        std::memcpy(map, shaderHandleStorage.data() + i * groupSize, groupSize);
    }
}
