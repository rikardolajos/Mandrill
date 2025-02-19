#pragma once

#include "Common.h"

#include "Device.h"
#include "Swapchain.h"

namespace Mandrill
{
    class Pass
    {
    public:
        /// <summary>
        /// Create a pass that is linked to a swapchain. This means that if the swapchain is recreated, the pass will
        /// match the extent of the new swapchain.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="pSwapchain">Swapchain to link with</param>
        /// <param name="depthAttachment">If depth attachment should be created</param>
        /// <param name="sampleCount">Multisampling count</param>
        /// <returns></returns>
        MANDRILL_API Pass(ptr<Device> pDevice, ptr<Swapchain> pSwapchain, bool depthAttachment = true,
                          VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

        /// <summary>
        /// Create a pass given a certain extent and format. The list of formats will create mathcing color attachments.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="extent">Resolution of attachments</param>
        /// <param name="formats">List of formats</param>
        /// <param name="depthAttachment">If depth attachment should be created</param>
        /// <param name="sampleCount">Multisampling count</param>
        /// <returns></returns>
        MANDRILL_API Pass(ptr<Device> pDevice, VkExtent2D extent, std::vector<VkFormat> formats,
                          bool depthAttachment = true, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

        /// <summary>
        /// Destroy a pass.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API ~Pass();

        /// <summary>
        /// Begin a pass without clearing the color attachments.
        /// </summary>
        /// <param name="cmd">Command buffer</param>
        /// <returns></returns>
        MANDRILL_API void begin(VkCommandBuffer cmd);

        /// <summary>
        /// Begin a pass with clearing of the color attachments.
        /// </summary>
        /// <param name="cmd">Command buffer</param>
        /// <param name="clearColor">Clearing color</param>
        /// <param name="loadOp">Load operation for color attachments</param>
        /// <returns></returns>
        MANDRILL_API void begin(VkCommandBuffer cmd, glm::vec4 clearColor,
                                VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR);

        /// <summary>
        /// Begin a pass but override the color attachments with an image. The image is expected to be usable as a color
        /// attachment.
        /// </summary>
        /// <param name="cmd">Command buffer</param>
        /// <param name="pImage">Overriding image</param>
        /// <returns></returns>
        MANDRILL_API void begin(VkCommandBuffer cmd, ptr<Image> pImage);

        /// <summary>
        /// End a pass.
        /// </summary>
        /// <param name="cmd">Command buffer</param>
        /// <returns></returns>
        MANDRILL_API void end(VkCommandBuffer cmd) const;

        /// <summary>
        /// End a pass and override the output image. The image will be transitioned for blitting.
        /// </summary>
        /// <param name="cmd">Command buffer</param>
        /// <param name="pImage">Overriding image</param>
        /// <returns></returns>
        MANDRILL_API void end(VkCommandBuffer cmd, ptr<Image> pImage) const;

        /// <summary>
        /// Get the pipeline rendering create info. This is needed for pipeline creation when using dynamic rendering.
        /// </summary>
        /// <returns>Pipeline rendering create info</returns>
        MANDRILL_API VkPipelineRenderingCreateInfo getPipelineRenderingCreateInfo() const
        {
            return mPipelineRenderingCreateInfo;
        }

        /// <summary>
        /// Get the list of color attachments of the pass.
        /// </summary>
        /// <returns>List of images</returns>
        MANDRILL_API const std::vector<ptr<Image>>& getColorAttachments() const
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

        /// <summary>
        /// Get the extent of the pass.
        /// </summary>
        /// <returns>Resolution of the attachments</returns>
        MANDRILL_API VkExtent2D getExtent() const
        {
            return mExtent;
        }

        /// <summary>
        /// Get the sample count of the pass. If VK_SAMPLE_COUNT_1_BIT is set, that means that there is no resolve
        /// happening in the pass.
        /// </summary>
        /// <returns>Sample count</returns>
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

        std::vector<VkFormat> mFormats;

        std::vector<ptr<Image>> mColorAttachments;
        ptr<Image> mpDepthAttachment;
        ptr<Image> mpResolveAttachment;
    };
} // namespace Mandrill
