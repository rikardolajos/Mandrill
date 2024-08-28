#pragma once

#include "Common.h"

#include "RenderPasses/RenderPass.h"

namespace Mandrill
{
    class Deferred : public RenderPass
    {
    public:
        MANDRILL_API Deferred(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                              std::vector<std::shared_ptr<Layout>> pLayouts,
                              std::vector<std::shared_ptr<Shader>> pShaders);

        MANDRILL_API ~Deferred();

        MANDRILL_API void frameBegin(VkCommandBuffer cmd, glm::vec4 clearColor) override;
        MANDRILL_API void frameEnd(VkCommandBuffer cmd) override;

        MANDRILL_API void nextSubpass(VkCommandBuffer cmd);

        /// <summary>
        /// Get the input attachment image descriptor info. Use this to send descriptors to command buffer.
        /// </summary>
        /// <param name="i">0 = Position, 1 = Normal, 2 = Albedo</param>
        /// <returns>A descriptor image info for the requrested attachment</returns>
        MANDRILL_API VkDescriptorImageInfo getInputAttachmentInfo(int i)
        {
            switch (i) {
            case 0:
                return VkDescriptorImageInfo{
                    .sampler = VK_NULL_HANDLE,
                    .imageView = mPosition->getImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
            case 1:
                return VkDescriptorImageInfo{
                    .sampler = VK_NULL_HANDLE,
                    .imageView = mNormal->getImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
            case 2:
                return VkDescriptorImageInfo{
                    .sampler = VK_NULL_HANDLE,
                    .imageView = mAlbedo->getImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
            default:
                return VkDescriptorImageInfo{};
            }
        }

    protected:
        void createPipelines() override;
        void destroyPipelines() override;

    private:
        void createRenderPass();
        void createAttachments();
        void createFramebuffers();
        void destroyFramebuffers();

        // VkImage mColor;
        // VkImageView mColorView;
        // VkImage mDepth;
        // VkImageView mDepthView;
        // VkDeviceMemory mResourceMemory;
        std::unique_ptr<Image> mPosition;
        std::unique_ptr<Image> mNormal;
        std::unique_ptr<Image> mAlbedo;
        std::unique_ptr<Image> mDepth;
        std::vector<VkFramebuffer> mFramebuffers;
    };
} // namespace Mandrill
