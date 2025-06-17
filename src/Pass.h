#pragma once

#include "Common.h"

#include "Device.h"
#include "Swapchain.h"

namespace Mandrill
{
    /// <summary>
    /// Pass class that abstracts the use of dynamic rendering (no render passes) in Vulkan. Passes can either be
    /// created with explicit color and depth attachments, or with implicit attachments that are created based on a
    /// given extent and format. The pass can be used to begin and end rendering commands, as well as transitioning
    /// images for rendering or blitting.
    /// </summary>
    class Pass
    {
    public:
        MANDRILL_NON_COPYABLE(Pass)

        /// <summary>
        /// Create a pass with explicit color and depth attachments.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="colorAttachments">Vector of image to use as color attachments</param>
        /// <param name="pDepthAttachment">Depth attachment to use, can be nullptr</param>
        MANDRILL_API Pass(ptr<Device> pDevice, std::vector<ptr<Image>> colorAttachments, ptr<Image> pDepthAttachment);

        /// <summary>
        /// Create a pass with implicit attachments, given a certain extent and format. Same format will be used for all
        /// color attachments.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="extent">Resolution of attachments</param>
        /// <param name="format">Format of color attachment</param>
        /// <param name="colorAttachmentCount">Number of color attachments</param>
        /// <param name="depthAttachment">If depth attachment should be created</param>
        /// <param name="sampleCount">Multisampling count</param>
        /// <returns></returns>
        MANDRILL_API Pass(ptr<Device> pDevice, VkExtent2D extent, VkFormat format, uint32_t colorAttachmentCount = 1,
                          bool depthAttachment = true, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

        /// <summary>
        /// Create a pass with implicit attachments, given a certain extent and format. The list of formats will create
        /// mathcing color attachments.
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
        /// Destructor for pass.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API ~Pass();

        /// <summary>
        /// Transition an image for rendering. Call before begin() in an explicit pass.
        /// </summary>
        /// <param name="cmd">Command buffer</param>
        /// <param name="pImage">Image to transition</param>
        /// <returns></returns>
        MANDRILL_API void transitionForRendering(VkCommandBuffer cmd, ptr<Image> pImage) const;

        /// <summary>
        /// Transition an image for blitting. Call after end() and before Swapchain::present(..., pImage) in an explicit
        /// pass.
        /// </summary>
        /// <param name="cmd">Command buffer</param>
        /// <param name="pImage">Image to transition</param>
        /// <returns></returns>
        MANDRILL_API void transitionForBlitting(VkCommandBuffer cmd, ptr<Image> pImage) const;

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
        /// End a pass and transition a given image for blitting.
        /// </summary>
        /// <param name="cmd">Command buffer</param>
        /// <param name="pImage">Image to transition</param>
        /// <returns></returns>
        MANDRILL_API void end(VkCommandBuffer cmd, ptr<Image> pImage) const;

        /// <summary>
        /// Update an explicit pass with new attachments. Typically call on swapchain recreation.
        /// </summary>
        /// <param name="colorAttachments">Vector with new color attachments</param>
        /// <param name="pDepthAttachment">New depth attachment, can be nullptr</param>
        /// <returns></returns>
        MANDRILL_API void update(std::vector<ptr<Image>> colorAttachments, ptr<Image> pDepthAttachment);

        /// <summary>
        /// Update an implicit pass with a new extent. Typically call on swapchain recreation.
        /// </summary>
        /// <param name="extent">New attachment extent</param>
        /// <returns></returns>
        MANDRILL_API void update(VkExtent2D extent);

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
        void createExplicitPass(std::vector<ptr<Image>> colorAttachments, ptr<Image> depthAttachment);
        void createImplicitPass(bool depthAttachment, VkSampleCountFlagBits sampleCount);

        ptr<Device> mpDevice;

        VkPipelineRenderingCreateInfo mPipelineRenderingCreateInfo;
        VkExtent2D mExtent;

        std::vector<VkFormat> mFormats;

        bool mImplicitAttachments;
        std::vector<ptr<Image>> mColorAttachments;
        ptr<Image> mpDepthAttachment;
        ptr<Image> mpResolveAttachment;
    };
} // namespace Mandrill
