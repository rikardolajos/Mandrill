#include "Rasterizer.h"

#include "Error.h"
#include "Helpers.h"
#include "Scene.h"

using namespace Mandrill;


Rasterizer::Rasterizer(ptr<Device> pDevice, ptr<Swapchain> pSwapchain) : RenderPass(pDevice, pSwapchain)
{
    mSampleCount = mpDevice->getSampleCount();

    createAttachments();
    createRenderPass();
    createFramebuffers();
}

Rasterizer::~Rasterizer()
{
    vkDeviceWaitIdle(mpDevice->getDevice());
    destroyFramebuffers();
    vkDestroyRenderPass(mpDevice->getDevice(), mRenderPass, nullptr);
}

void Rasterizer::createRenderPass()
{
    VkAttachmentDescription colorAttachment = {
        .format = mpSwapchain->getImageFormat(),
        .samples = mSampleCount,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkFormat depthFormat = Helpers::findDepthFormat(mpDevice);
    VkAttachmentDescription depthAttachment = {
        .format = depthFormat,
        .samples = mSampleCount,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription colorAttachmentResolve = {
        .format = mpSwapchain->getImageFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthAttachmentRef = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference colorAttachmentResolveRef = {
        .attachment = 2,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pResolveAttachments = &colorAttachmentResolveRef,
        .pDepthStencilAttachment = &depthAttachmentRef,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    std::array<VkAttachmentDescription, 3> attachments = {
        colorAttachment,
        depthAttachment,
        colorAttachmentResolve,
    };

    VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    Check::Vk(vkCreateRenderPass(mpDevice->getDevice(), &ci, nullptr, &mRenderPass));
}

void Rasterizer::createAttachments()
{
    VkFormat format = mpSwapchain->getImageFormat();
    VkFormat depthFormat = Helpers::findDepthFormat(mpDevice);
    VkExtent2D extent = mpSwapchain->getExtent();

    mColor = make_ptr<Image>(mpDevice, extent.width, extent.height, 1, mSampleCount, format,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mDepth = make_ptr<Image>(mpDevice, extent.width, extent.height, 1, mSampleCount, depthFormat,
                             VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Helpers::transitionImageLayout(mpDevice, mDepth->getImage(), depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);

    mColor->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
    mDepth->createImageView(VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Rasterizer::destroyAttachments()
{
    mColor = nullptr;
    mDepth = nullptr;
}

void Rasterizer::createFramebuffers()
{
    auto imageViews = mpSwapchain->getImageViews();
    mFramebuffers = std::vector<VkFramebuffer>(imageViews.size());

    for (uint32_t i = 0; i < mFramebuffers.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            mColor->getImageView(),
            mDepth->getImageView(),
            imageViews[i],
        };

        VkFramebufferCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = mRenderPass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = mpSwapchain->getExtent().width,
            .height = mpSwapchain->getExtent().height,
            .layers = 1,
        };

        Check::Vk(vkCreateFramebuffer(mpDevice->getDevice(), &ci, nullptr, &mFramebuffers[i]));
    }
}

void Rasterizer::destroyFramebuffers()
{
    for (auto& fb : mFramebuffers) {
        vkDestroyFramebuffer(mpDevice->getDevice(), fb, nullptr);
    }
}

void Rasterizer::frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor)
{
    if (mpSwapchain->recreated()) {
        Log::debug("Recreating framebuffers since swapchain changed");
        destroyFramebuffers();
        destroyAttachments();
        createAttachments();
        createFramebuffers();
    }

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0].color = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = mRenderPass,
        .framebuffer = mFramebuffers[mpSwapchain->getImageIndex()],
        .renderArea =
            {
                .offset = {0, 0},
                .extent = mpSwapchain->getExtent(),
            },
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
}

void Rasterizer::frameEnd(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}
