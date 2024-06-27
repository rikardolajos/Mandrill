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
        MANDRILL_API void createFramebuffers(VkRenderPass renderPass);

        MANDRILL_API VkCommandBuffer acquireNextImage();
        MANDRILL_API void present();

        MANDRILL_API VkSwapchainKHR getSwapchain() const
        {
            return mSwapchain;
        }

        MANDRILL_API VkFormat getImageFormat() const
        {
            return mImageFormat;
        }

        MANDRILL_API VkExtent2D getExtent() const
        {
            return mExtent;
        }

        MANDRILL_API VkFramebuffer getCurrentFramebuffer() const
        {
            return mFramebuffers[mImageIndex];
        }

        MANDRILL_API bool recreated() const
        {
            return mRecreated;
        }

        MANDRILL_API void recreationAcknowledged()
        {
            mRecreated = false;
        }

    private:
        void querySupport();
        void createResources();
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

        VkImage mColor;
        VkImageView mColorView;
        VkImage mDepth;
        VkImageView mDepthView;
        VkDeviceMemory mResourceMemory;

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