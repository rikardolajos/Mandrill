#pragma once

#include "Common.h"

#include "Device.h"
#include "Swapchain.h"

namespace Mandrill
{
    class Pass
    {
    public:
        MANDRILL_API Pass(ptr<Device> pDevice, ptr<Swapchain> pSwapchain, bool depthAttachment = true,
                          VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
        MANDRILL_API Pass(ptr<Device> pDevice, VkExtent2D extent, Vector<VkFormat> formats, bool depthAttachment = true,
                          VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
        MANDRILL_API ~Pass();

        MANDRILL_API void begin(VkCommandBuffer cmd);
        MANDRILL_API void begin(VkCommandBuffer cmd, glm::vec4 clearColor,
                                VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR);
        MANDRILL_API void end(VkCommandBuffer cmd) const;

        MANDRILL_API VkPipelineRenderingCreateInfo getPipelineRenderingCreateInfo() const
        {
            return mPipelineRenderingCreateInfo;
        }

        MANDRILL_API const Vector<ptr<Image>>& getColorAttachments() const
        {
            return mColorAttachments;
        }

        /// <summary>
        /// Get the output image of the pass. If multi-sampling was used, the resolve images is returned, otherwise the
        /// first color attachment is assumed to be the output.
        /// </summary>
        /// <returns>Pass output image</returns>
        MANDRILL_API ptr<Image> getOutput() const
        {
            return mpResolveAttachment ? mpResolveAttachment : mColorAttachments[0];
        }

        MANDRILL_API VkExtent2D getExtent() const
        {
            return mExtent;
        }

        MANDRILL_API VkSampleCountFlagBits getSampleCount() const
        {
            return mpResolveAttachment ? mpDevice->getSampleCount() : VK_SAMPLE_COUNT_1_BIT;
        }

    private:
        void createPass(bool depthAttachment, VkSampleCountFlagBits sampleCount);

        ptr<Device> mpDevice;
        ptr<Swapchain> mpSwapchain;

        VkPipelineRenderingCreateInfo mPipelineRenderingCreateInfo;
        VkExtent2D mExtent;

        Vector<VkFormat> mFormats;

        Vector<ptr<Image>> mColorAttachments;
        ptr<Image> mpDepthAttachment;
        ptr<Image> mpResolveAttachment;
    };
} // namespace Mandrill
