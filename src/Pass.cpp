#include "Pass.h"

#include "Helpers.h"

using namespace Mandrill;

Pass::Pass(ptr<Device> pDevice, std::vector<ptr<Image>> colorAttachments, ptr<Image> pDepthAttachment)
    : mpDevice(pDevice), mImplicitAttachments(false)
{
    createExplicitPass(colorAttachments, pDepthAttachment);
}

Pass::Pass(ptr<Device> pDevice, VkExtent2D extent, VkFormat format, uint32_t colorAttachmentCount, bool depthAttachment,
           VkSampleCountFlagBits sampleCount)
    : mpDevice(pDevice), mImplicitAttachments(true), mExtent(extent)
{
    mFormats = {format};
    createImplicitPass(depthAttachment, sampleCount);
}

Pass::Pass(ptr<Device> pDevice, VkExtent2D extent, std::vector<VkFormat> formats, bool depthAttachment,
           VkSampleCountFlagBits sampleCount)
    : mpDevice(pDevice), mImplicitAttachments(true), mExtent(extent), mFormats(formats)
{
    createImplicitPass(depthAttachment, sampleCount);
}

Pass::~Pass()
{
}

void Pass::transitionForRendering(VkCommandBuffer cmd, ptr<Image> pImage) const
{
    Helpers::imageBarrier(cmd, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, pImage->getImage());
}

void Pass::transitionForBlitting(VkCommandBuffer cmd, ptr<Image> pImage) const
{
    Helpers::imageBarrier(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          pImage->getImage());
}

void Pass::begin(VkCommandBuffer cmd)
{
    begin(cmd, glm::vec4(0.0f), VK_ATTACHMENT_LOAD_OP_LOAD);
}

void Pass::begin(VkCommandBuffer cmd, glm::vec4 clearColor, VkAttachmentLoadOp loadOp)
{
    if (mImplicitAttachments) {
        transitionForRendering(cmd, mpResolveAttachment ? mpResolveAttachment : mColorAttachments[0]);
    }

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

void Pass::begin(VkCommandBuffer cmd, ptr<Image> pImage)
{
    VkRenderingAttachmentInfo colorAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = pImage->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = mpResolveAttachment ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = mpResolveAttachment ? mpResolveAttachment->getImageView() : nullptr,
        .resolveImageLayout =
            mpResolveAttachment ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =
            {
                .color = {0.0f, 0.0f, 0.0f, 0.0f},
            },
    };

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

    VkExtent2D extent = {
        .width = pImage->getWidth(),
        .height = pImage->getHeight(),
    };

    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            {
                .offset = {0, 0},
                .extent = extent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = mpDepthAttachment ? &depthAttachmentInfo : nullptr,
        .pStencilAttachment = nullptr,
    };

    vkCmdBeginRendering(cmd, &renderingInfo);
}

void Pass::end(VkCommandBuffer cmd) const
{
    end(cmd, nullptr);
}

void Pass::end(VkCommandBuffer cmd, ptr<Image> pImage) const
{
    vkCmdEndRendering(cmd);

    // If image is provided, transition it
    if (pImage) {
        transitionForBlitting(cmd, pImage);
        return;
    }

    // If doing implicit pass, transition the implicit ouput
    if (mImplicitAttachments) {
        transitionForBlitting(cmd, mpResolveAttachment ? mpResolveAttachment : mColorAttachments[0]);
        return;
    }
}

void Pass::update(std::vector<ptr<Image>> colorAttachments, ptr<Image> pDepthAttachment)
{
    if (mImplicitAttachments) {
        Log::error("Cannot use explicit update for an implicit pass");
        return;
    }
    createExplicitPass(colorAttachments, pDepthAttachment);
}

void Pass::update(VkExtent2D extent)
{
    if (!mImplicitAttachments) {
        Log::error("Cannot use implicit update for an explicit pass");
        return;
    }
    mExtent = extent;
    bool depthAttachment = !!mpDepthAttachment;
    VkSampleCountFlagBits sampleCount = getSampleCount();
    createImplicitPass(depthAttachment, sampleCount);
}

void Pass::createExplicitPass(std::vector<ptr<Image>> colorAttachments, ptr<Image> pDepthAttachment)
{
    mExtent.width = colorAttachments[0]->getWidth();
    mExtent.height = colorAttachments[0]->getHeight();

    VkFormat depthFormat = Helpers::findDepthFormat(mpDevice);

    mColorAttachments.clear();

    for (auto& attachment : colorAttachments) {
        mColorAttachments.push_back(attachment);
        mFormats.push_back(attachment->getFormat());
    }

    if (pDepthAttachment) {
        mpDepthAttachment = pDepthAttachment;
        mpDepthAttachment->createImageView(VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    mPipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = count(mColorAttachments),
        .pColorAttachmentFormats = mFormats.data(),
        .depthAttachmentFormat = mpDepthAttachment->getFormat(),
    };
}

void Pass::createImplicitPass(bool depthAttachment, VkSampleCountFlagBits sampleCount)
{
    VkFormat depthFormat = Helpers::findDepthFormat(mpDevice);

    mColorAttachments.clear();

    for (auto format : mFormats) {
        VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        mColorAttachments.push_back(make_ptr<Image>(mpDevice, mExtent.width, mExtent.height, 1, sampleCount, format,
                                                    VK_IMAGE_TILING_OPTIMAL, usage,
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

        Helpers::transitionImageLayout(mpDevice, mColorAttachments.back()->getImage(), format,
                                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

        mColorAttachments.back()->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
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
