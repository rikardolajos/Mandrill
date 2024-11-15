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

RayTracingPipeline::RayTracingPipeline(ptr<Device> pDevice, ptr<Shader> pShader, ptr<Layout> pLayout)
    : mpDevice(pDevice), mpShader(pShader), mpLayout(pLayout)
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

    createPipeline();
}

RayTracingPipeline::~RayTracingPipeline()
{
    destroyPipeline();
}

void RayTracingPipeline::bind(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
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

void RayTracingPipeline::recreate()
{
    destroyPipeline();
    mpShader->reload();
    createPipeline();
}

void RayTracingPipeline::createPipeline()
{
    VkRayTracingPipelineCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(mpShader->getStages().size()),
        .pStages = mpShader->getStages().data(),
        .groupCount = static_cast<uint32_t>(mShaderGroups.size()),
        .pGroups = mShaderGroups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = mPipelineLayout,
    };

    vkCreateRayTracingPipelinesKHR(mpDevice->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &mPipeline);
}

void RayTracingPipeline::destroyPipeline()
{
    vkDeviceWaitIdle(mpDevice->getDevice());
    vkDestroyPipeline(mpDevice->getDevice(), mPipeline, nullptr);
}

void RayTracingPipeline::createShaderBindingTable()
{
    uint32_t groupSize = mpDevice->getProperties().rayTracingPipeline.shaderGroupHandleSize;
    uint32_t groupSizeAligned =
        Helpers::alignTo(groupSize, mpDevice->getProperties().rayTracingPipeline.shaderGroupBaseAlignment);

    // Bytes needed for the shader binding table
    uint32_t sbtSize = mShaderGroups.size() * groupSizeAligned;
    auto shaderHandleStorage = std::vector<uint8_t>(sbtSize);

    Check::Vk(vkGetRayTracingShaderGroupHandlesKHR(mpDevice->getDevice(), mPipeline, 0, SHADER_GROUP_COUNT, sbtSize,
                                                   shaderHandleStorage.data()));

    // Allocate buffer for the shader binding table
    mpShaderBindingTableBuffer =
        make_ptr<Buffer>(mpDevice, sbtSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                             VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Write the handles at the aligned positions
    for (uint32_t i = 0; i < SHADER_GROUP_COUNT; i++) {
        uint8_t* map = static_cast<uint8_t*>(mpShaderBindingTableBuffer->getHostMap());
        std::memcpy(map, shaderHandleStorage.data() + i * groupSize, groupSize);
    }
}

//
// void RayTracingPipeline::createRenderPass()
//{
//}
//
// void RayTracingPipeline::createAttachments()
//{
//    //// Create descriptor
//    // std::vector<DescriptorDesc> descriptorDesc;
//
//    // auto imageViews = mpSwapchain->getImageViews();
//    // for (uint32_t i = 0; i < imageViews.size(); i++) {
//    //     descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr);
//    //     (descriptorDesc.end() - 1)->imageView = imageViews[i];
//    // }
//
//    // mpAttachmentDescriptor =
//    //     make_ptr<Descriptor>(mpDevice, descriptorDesc, mpLayouts[0]->getDescriptorSetLayouts()[0], 1);
//}


// void RayTracingPipeline::frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor)
//{
//     if (mpSwapchain->recreated()) {
//         Log::debug("Recreating framebuffers since swapchain changed");
//         destroyFramebuffers();
//         destroyAttachments();
//         createAttachments();
//         createFramebuffers();
//     }
//
//     VkCommandBufferBeginInfo bi = {
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
//         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
//     };
//
//     Check::Vk(vkBeginCommandBuffer(cmd, &bi));

// vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[0]);

//// Set dynamic states
// vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
// vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

//// Transition storage image to format for writing
// VkImageMemoryBarrier barrier = {
//     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
//     .newLayout = VK_IMAGE_LAYOUT_GENERAL,
//     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//     .image = mpSwapchain->getImage(),
//     .subresourceRange =
//         {
//             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//             .baseMipLevel = 0,
//             .levelCount = 1,
//             .baseArrayLayer = 0,
//             .layerCount = 1,
//         },
// };

// vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0,
//                      nullptr, 0, nullptr, 1, &barrier);
//}

// void RayTracingPipeline::frameEnd(VkCommandBuffer cmd)
//{
//     // Transition storage image to format for presenting
//     VkImageMemoryBarrier barrier = {
//         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
//         .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
//         .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
//         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//         .image = mpSwapchain->getImage(),
//         .subresourceRange =
//             {
//                 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//                 .baseMipLevel = 0,
//                 .levelCount = 1,
//                 .baseArrayLayer = 0,
//                 .layerCount = 1,
//             },
//     };
//
//     vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
//     0,
//                          nullptr, 0, nullptr, 1, &barrier);
//
//     Check::Vk(vkEndCommandBuffer(cmd));
// }
