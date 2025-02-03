#include "RayTracing.h"

#include "Error.h"
#include "Helpers.h"
#include "Scene.h"

using namespace Mandrill;


RayTracing::RayTracing(ptr<Device> pDevice, ptr<Swapchain> pSwapchain) : RenderPass(pDevice, pSwapchain)
{
    mSampleCount = mpDevice->getSampleCount();
    mSampleCount = VK_SAMPLE_COUNT_1_BIT;

    createRenderPass();
    createFramebuffers();
}

RayTracing::~RayTracing()
{
    vkDeviceWaitIdle(mpDevice->getDevice());
    destroyFramebuffers();
    vkDestroyRenderPass(mpDevice->getDevice(), mRenderPass, nullptr);
}

void RayTracing::createRenderPass()
{
    VkAttachmentDescription colorAttachment = {
        .format = mpSwapchain->getImageFormat(),
        .samples = mSampleCount,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, // Preserve ray-traced image
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
    };

    VkRenderPassCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    Check::Vk(vkCreateRenderPass(mpDevice->getDevice(), &ci, nullptr, &mRenderPass));
}

void RayTracing::createAttachments()
{
}

void RayTracing::destroyAttachments()
{
}

void RayTracing::createFramebuffers()
{
    auto imageViews = mpSwapchain->getImageViews();
    mFramebuffers = std::vector<VkFramebuffer>(imageViews.size());

    for (uint32_t i = 0; i < mFramebuffers.size(); i++) {
        std::array<VkImageView, 1> attachments = {
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

void RayTracing::destroyFramebuffers()
{
    for (auto& fb : mFramebuffers) {
        vkDestroyFramebuffer(mpDevice->getDevice(), fb, nullptr);
    }
}

void RayTracing::begin(VkCommandBuffer cmd, glm::vec4 clearColor)
{
    if (mpSwapchain->recreated()) {
        Log::debug("Recreating framebuffers since swapchain changed");
        destroyFramebuffers();
        destroyAttachments();
        createAttachments();
        createFramebuffers();
    }

    std::array<VkClearValue, 1> clearValues = {};
    clearValues[0].color = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]};

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

void RayTracing::end(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}
