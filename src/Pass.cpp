#include "Pass.h"

#include "Helpers.h"

using namespace Mandrill;

Pass::Pass(ptr<Device> pDevice, ptr<Swapchain> pSwapchain, bool depthAttachment, VkSampleCountFlagBits sampleCount)
    : Pass(pDevice, pSwapchain->getExtent(), pSwapchain->getImageFormats(), depthAttachment, sampleCount)
{
    mpSwapchain = pSwapchain;
}

Pass::Pass(ptr<Device> pDevice, VkExtent2D extent, std::vector<VkFormat> formats, bool depthAttachment,
           VkSampleCountFlagBits sampleCount)
    : mpDevice(pDevice), mExtent(extent), mFormats(formats)
{
    createPass(depthAttachment, sampleCount);
}

Pass::~Pass()
{
}

void Pass::begin(VkCommandBuffer cmd)
{
    begin(cmd, glm::vec4(0.0f), VK_ATTACHMENT_LOAD_OP_LOAD);
}

void Pass::begin(VkCommandBuffer cmd, glm::vec4 clearColor, VkAttachmentLoadOp loadOp)
{
    if (mpSwapchain && mpSwapchain->recreated()) {
        bool depthAttachment = !!mpDepthAttachment;
        VkSampleCountFlagBits sampleCount = getSampleCount();
        createPass(depthAttachment, sampleCount);
    }

    // Transition output image for rendering
    VkImageMemoryBarrier2 srcBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = mpResolveAttachment ? mpResolveAttachment->getImage() : mColorAttachments[0]->getImage(),
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkDependencyInfo srcDependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &srcBarrier,
    };

    vkCmdPipelineBarrier2(cmd, &srcDependencyInfo);

    // Set up attachments and begin
    std::vector<VkRenderingAttachmentInfo> colorAttachmentInfos;
    for (auto colorAttachment : mColorAttachments) {
        VkRenderingAttachmentInfo colorAttachmentInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = colorAttachment->getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = mpResolveAttachment ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
            .resolveImageView = mpResolveAttachment ? mpResolveAttachment->getImageView() : nullptr,
            .resolveImageLayout =
                mpResolveAttachment ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp = loadOp,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue =
                {
                    .color = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]},
                },
        };
        colorAttachmentInfos.push_back(colorAttachmentInfo);
    }

    VkRenderingAttachmentInfo depthAttachmentInfo;
    if (mpDepthAttachment) {
        depthAttachmentInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = mpDepthAttachment->getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.depthStencil = {.depth = 1.0f, .stencil = 0}},
        };
    }

    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            {
                .offset = {0, 0},
                .extent = mExtent,
            },
        .layerCount = 1,
        .colorAttachmentCount = count(colorAttachmentInfos),
        .pColorAttachments = colorAttachmentInfos.data(),
        .pDepthAttachment = mpDepthAttachment ? &depthAttachmentInfo : nullptr,
        .pStencilAttachment = nullptr,
    };

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void Pass::end(VkCommandBuffer cmd) const
{
    // End rendering (and resolve image)
    vkCmdEndRendering(cmd);

    // Transition output image for blitting
    VkImageMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = mpResolveAttachment ? mpResolveAttachment->getImage() : mColorAttachments[0]->getImage(),
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

void Pass::createPass(bool depthAttachment, VkSampleCountFlagBits sampleCount)
{
    // Update the extent from the swapchain, if coupled
    if (mpSwapchain) {
        mExtent = mpSwapchain->getExtent();
    }

    VkFormat depthFormat = Helpers::findDepthFormat(mpDevice);

    mColorAttachments.clear();

    for (auto format : mFormats) {
        mColorAttachments.push_back(make_ptr<Image>(
            mpDevice, mExtent.width, mExtent.height, 1, sampleCount, format, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

        Helpers::transitionImageLayout(mpDevice, mColorAttachments.back()->getImage(), format,
                                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

        mColorAttachments.back()->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
        mColorAttachments.back()->createDescriptor();
    }

    if (sampleCount != VK_SAMPLE_COUNT_1_BIT) {
        mpResolveAttachment = make_ptr<Image>(mpDevice, mExtent.width, mExtent.height, 1, VK_SAMPLE_COUNT_1_BIT,
                                              VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mpResolveAttachment->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
    }

    if (depthAttachment) {
        mpDepthAttachment = make_ptr<Image>(mpDevice, mExtent.width, mExtent.height, 1, sampleCount, depthFormat,
                                            VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        Helpers::transitionImageLayout(mpDevice, mpDepthAttachment->getImage(), depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

        mpDepthAttachment->createImageView(VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    mPipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = count(mColorAttachments),
        .pColorAttachmentFormats = mFormats.data(),
        .depthAttachmentFormat = depthFormat,
    };
}
