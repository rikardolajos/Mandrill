#include "RayTracer.h"

#include "Error.h"
#include "Extension.h"
#include "Helpers.h"
#include "Scene.h"

using namespace Mandrill;

RayTracer::RayTracer(ptr<Device> pDevice, ptr<Swapchain> pSwapchain, const RenderPassDesc& desc)
    : RenderPass(pDevice, pSwapchain, desc)
{
    if (mpLayouts.size() != 1 or mpShaders.size() != 1) {
        Log::error("RayTracer only supports one render pass and was created with wrong number of layouts and shaders");
    }

    createAttachments();
    createRenderPass();
    createFramebuffers();
    createPipelines();
}

RayTracer::~RayTracer()
{
    vkDeviceWaitIdle(mpDevice->getDevice());
    destroyPipelines();
    destroyFramebuffers();
    vkDestroyRenderPass(mpDevice->getDevice(), mRenderPass, nullptr);
}

void RayTracer::createPipelines()
{
    VkRayTracingPipelineCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(mpShaders[0]->getStages().size()),
        .pStages = mpShaders[0]->getStages().data(),
        .groupCount = static_cast<uint32_t>(mpShaderGroups.size()),
        .pGroups = mpShaderGroups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = mPipelineLayouts[0],
    };

    vkCreateRayTracingPipelinesKHR(mpDevice->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr,
                                   mPipelines.data());
}

void RayTracer::destroyPipelines()
{
    for (auto pipeline : mPipelines) {
        vkDestroyPipeline(mpDevice->getDevice(), pipeline, nullptr);
    }
}

void RayTracer::createRenderPass()
{
}

void RayTracer::createAttachments()
{
    // Create descriptor
    std::vector<DescriptorDesc> descriptorDesc;

    auto imageViews = mpSwapchain->getImageViews();
    for (uint32_t i = 0; i < imageViews.size(); i++) {
        descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr);
        (descriptorDesc.end() - 1)->imageView = imageViews[i];
    }

    mpAttachmentDescriptor =
        make_ptr<Descriptor>(mpDevice, descriptorDesc, mpLayouts[0]->getDescriptorSetLayouts()[0], 1);
}

void RayTracer::destroyAttachments()
{
}

void RayTracer::createFramebuffers()
{
}

void RayTracer::destroyFramebuffers()
{
}

void RayTracer::frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor)
{
    if (mpSwapchain->recreated()) {
        Log::debug("Recreating framebuffers since swapchain changed");
        destroyFramebuffers();
        destroyAttachments();
        createAttachments();
        createFramebuffers();
    }

    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    Check::Vk(vkBeginCommandBuffer(cmd, &bi));

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[0]);

    // Set dynamic states
    vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

    // Transition storage image to format for writing
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = mpSwapchain->getImage(),
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

void RayTracer::frameEnd(VkCommandBuffer cmd)
{
    // Transition storage image to format for presenting
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = mpSwapchain->getImage(),
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    Check::Vk(vkEndCommandBuffer(cmd));
}
