#pragma once

#include "Common.h"

#include "Device.h"
#include "Image.h"

namespace Mandrill
{
    class Swapchain
    {
    public:
        MANDRILL_API Swapchain(std::shared_ptr<Device> pDevice, uint32_t framesInFlight = 2);
        MANDRILL_API ~Swapchain();

        MANDRILL_API void recreate();

        MANDRILL_API VkCommandBuffer acquireNextImage();
        MANDRILL_API void present();

        MANDRILL_API VkSwapchainKHR getSwapchain() const
        {
            return mSwapchain;
        }

        MANDRILL_API VkImage getImage() const
        {
            return mImages[mImageIndex];
        }

        MANDRILL_API std::vector<VkImageView> getImageViews() const
        {
            return mImageViews;
        }

        MANDRILL_API VkFormat getImageFormat() const
        {
            return mImageFormat;
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

        MANDRILL_API uint32_t getFramesInFlightCount() const
        {
            return static_cast<uint32_t>(mInFlightFences.size());
        }

        MANDRILL_API bool recreated() const
        {
            return mRecreated;
        }

    private:
        void querySupport();
        void createSwapchain();
        void destroySwapchain();
        void createSyncObjects(uint32_t framesInFlight);
        void destroySyncObjects();

        std::shared_ptr<Device> mpDevice;

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

        std::vector<VkSemaphore> mImageAvailableSemaphore;
        std::vector<VkSemaphore> mRenderFinishedSemaphore;
        std::vector<VkFence> mInFlightFences;

        uint32_t mInFlightIndex = 0;
        uint32_t mImageIndex = 0;

        bool mRecreated = false;
    };
} // namespace Mandrill