#pragma once

#include "Common.h"

#include "Device.h"
#include "Error.h"
#include "Image.h"

namespace Mandrill
{
    class Swapchain
    {
    public:
        MANDRILL_API Swapchain(ptr<Device> pDevice, uint32_t framesInFlight = 2);
        MANDRILL_API ~Swapchain();

        MANDRILL_API void recreate();

        MANDRILL_API VkCommandBuffer acquireNextImage();
        MANDRILL_API void present(VkCommandBuffer cmd, ptr<Image> pImage);

        MANDRILL_API VkSwapchainKHR getSwapchain() const
        {
            return mSwapchain;
        }

        MANDRILL_API VkImage getImage() const
        {
            return mImages[mImageIndex];
        }

        MANDRILL_API const std::vector<VkImage>& getImages() const
        {
            return mImages;
        }

        MANDRILL_API VkImageView getImageView() const
        {
            return mImageViews[mImageIndex];
        }

        MANDRILL_API const std::vector<VkImageView>& getImageViews() const
        {
            return mImageViews;
        }

        MANDRILL_API VkDescriptorSet getImageDescriptorSet() const
        {
            return mDescriptorSets[mImageIndex];
        }

        MANDRILL_API VkFormat getImageFormat() const
        {
            return mImageFormat;
        }

        MANDRILL_API std::vector<VkFormat> getImageFormats() const
        {
            std::vector<VkFormat> formats;
            formats.push_back(mImageFormat);
            return formats;
        }

        MANDRILL_API VkExtent2D getExtent() const
        {
            return mExtent;
        }

        MANDRILL_API uint32_t getImageIndex() const
        {
            return mImageIndex;
        }

        MANDRILL_API uint32_t getInFlightIndex() const
        {
            return mInFlightIndex;
        }

        /// <summary>
        /// Get the index of the previous frame that was in flight. This is useful when indexing after calling
        /// present(), like when querying for timestamps.
        /// </summary>
        /// <returns>Index of previous frame in flight</returns>
        MANDRILL_API uint32_t getPreviousInFlightIndex() const
        {
            return mPreviousInFlightIndex;
        }

        MANDRILL_API uint32_t getFramesInFlightCount() const
        {
            return static_cast<uint32_t>(mInFlightFences.size());
        }

        MANDRILL_API bool recreated() const
        {
            return mRecreated;
        }

        MANDRILL_API void waitForInFlightImage() const
        {
            Check::Vk(vkWaitForFences(mpDevice->getDevice(), 1, &mInFlightFences[mInFlightIndex], VK_TRUE, UINT64_MAX));
        }

    private:
        void querySupport();
        void createSwapchain();
        void destroySwapchain();
        void createSyncObjects(uint32_t framesInFlight);
        void destroySyncObjects();
        void createDescriptor();
        void destroyDescriptor();

        ptr<Device> mpDevice;

        VkSwapchainKHR mSwapchain;
        VkFormat mImageFormat;
        VkExtent2D mExtent;
        std::vector<VkFramebuffer> mFramebuffers;

        std::vector<VkImage> mImages;
        std::vector<VkImageView> mImageViews;

        struct {
            VkSurfaceCapabilitiesKHR capabilities{};
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        } mSupportDetails;

        std::vector<VkCommandBuffer> mCommandBuffers;

        std::vector<VkSemaphore> mImageAvailableSemaphores;
        std::vector<VkSemaphore> mRenderFinishedSemaphores;
        std::vector<VkFence> mInFlightFences;

        uint32_t mInFlightIndex = 0;
        uint32_t mPreviousInFlightIndex = 0;
        uint32_t mImageIndex = 0;

        VkDescriptorSetLayout mDescriptorSetLayout;
        VkDescriptorPool mDescriptorPool;
        std::vector<VkDescriptorSet> mDescriptorSets;

        bool mRecreated = false;
    };
} // namespace Mandrill