#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "Error.h"
#include "Image.h"

namespace Mandrill
{
    /// <summary>
    /// Swapchain class for managing the swapchain and its images.
    /// </summary>
    class Swapchain
    {
    public:
        MANDRILL_NON_COPYABLE(Swapchain)

        /// <summary>
        /// Create a new swapchain.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="framesInFlight">How many frames in flight that should be used</param>
        MANDRILL_API Swapchain(ptr<Device> pDevice, uint32_t framesInFlight = 2);

        /// <summary>
        /// Destructor for swapchain.
        /// </summary>
        MANDRILL_API ~Swapchain();

        /// <summary>
        /// Recreate the swapchain, for instance if window size changed.
        /// </summary>
        MANDRILL_API void recreate();

        /// <summary>
        /// Wait for the fence of the current frame in flight to signal. Call this before using resources that are
        /// shared between host and device. acquireNextImage() will automatically call this.
        /// </summary>
        MANDRILL_API void waitForFence();

        /// <summary>
        /// Acquire the next image in the swapchain to render to.
        /// </summary>
        /// <returns>Command buffer to use to render to the new swapchain image</returns>
        MANDRILL_API VkCommandBuffer acquireNextImage();

        /// <summary>
        /// Present a rendered image to the swapchain image.
        /// </summary>
        /// <param name="cmd">Command buffer which was acquired with acquireNextImage()</param>
        /// <param name="pImage">Image to blit to the swapchain image</param>
        MANDRILL_API void present(VkCommandBuffer cmd, ptr<Image> pImage);

        /// <summary>
        /// Request a screenshot from the next rendered frame. This call must be paired with a call to
        /// waitForScreenshot(). See App::takeScreenshot() for proper usage.
        /// </summary>
        MANDRILL_API void requestScreenshot();

        /// <summary>
        /// Wait for screenshot to be available and acquire it. Only call this after a call to requestScreenshot(). See
        /// App::takeScreenshot() for proper usage.
        /// </summary>
        /// <returns>Vector with pixel data</returns>
        MANDRILL_API std::vector<uint8_t> waitForScreenshot();

        /// <summary>
        /// Get the swapchain handle.
        /// </summary>
        /// <returns>Swapchain handle</returns>
        MANDRILL_API VkSwapchainKHR getSwapchain() const
        {
            return mSwapchain;
        }

        /// <summary>
        /// Get the current swapchain image.
        /// </summary>
        /// <returns>Current swapchain image handle</returns>
        MANDRILL_API VkImage getImage() const
        {
            return mImages[mImageIndex];
        }

        /// <summary>
        /// Get all the swapchain images.
        /// </summary>
        /// <returns>Vector of swapchain image handles</returns>
        MANDRILL_API const std::vector<VkImage>& getImages() const
        {
            return mImages;
        }

        /// <summary>
        /// Get the current swapchain image view
        /// </summary>
        /// <returns>Current swapchain image view</returns>
        MANDRILL_API VkImageView getImageView() const
        {
            return mImageViews[mImageIndex];
        }

        /// <summary>
        /// Get all the swapchain image views.
        /// </summary>
        /// <returns>Vector of swapchain image view handles</returns>
        MANDRILL_API const std::vector<VkImageView>& getImageViews() const
        {
            return mImageViews;
        }

        /// <summary>
        /// Get the descriptor set of the current swapchain image. Useful if using the swapchain image as a storage
        /// image in a shader.
        /// </summary>
        /// <returns>Descriptor set handle</returns>
        MANDRILL_API VkDescriptorSet getImageDescriptorSet() const
        {
            return mDescriptorSets[mImageIndex];
        }

        /// <summary>
        /// Get the image format of the swapchain.
        /// </summary>
        /// <returns>Image format</returns>
        MANDRILL_API VkFormat getImageFormat() const
        {
            return mImageFormat;
        }

        /// <summary>
        /// Get the extent (resolution) of the swapchain
        /// </summary>
        /// <returns>Image extent</returns>
        MANDRILL_API VkExtent2D getExtent() const
        {
            return mExtent;
        }

        /// <summary>
        /// Get the current image index.
        /// </summary>
        /// <returns>Index to current swapchain image</returns>
        MANDRILL_API uint32_t getImageIndex() const
        {
            return mImageIndex;
        }

        /// <summary>
        /// Get the current frame in flight index.
        /// </summary>
        /// <returns>Index to the current frame in flight</returns>
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

        /// <summary>
        /// Get the count of the total number of frames in flight.
        /// </summary>
        /// <returns>Number of frames in flight</returns>
        MANDRILL_API uint32_t getFramesInFlightCount() const
        {
            return static_cast<uint32_t>(mInFlightFences.size());
        }

        /// <summary>
        /// Check if the swapchain was recreated during the last frame. This is reset in present().
        /// </summary>
        /// <returns>True if recreated, otherwise false</returns>
        MANDRILL_API bool recreated() const
        {
            return mRecreated;
        }

        /// <summary>
        /// Get the pitch (number of bytes per row) of the screenshot stage image.
        /// </summary>
        /// <returns>Pitch in bytes</returns>
        MANDRILL_API uint32_t getScreenshotImagePitch() const
        {
            return mScreenshotStageImage->getPitch();
        }

    private:
        void querySupport();
        void createSwapchain();
        void destroySwapchain();
        void createSyncObjects(uint32_t framesInFlight);
        void destroySyncObjects();
        void createDescriptor();
        void destroyDescriptor();
        void createScreenshotStageImage();

        ptr<Device> mpDevice;

        VkSwapchainKHR mSwapchain;
        VkFormat mImageFormat;
        VkExtent2D mExtent;

        std::vector<VkImage> mImages;
        std::vector<VkImageView> mImageViews;

        struct {
            VkSurfaceCapabilitiesKHR capabilities{};
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        } mSupportDetails;

        std::vector<VkCommandBuffer> mCommandBuffers;

        std::vector<VkSemaphore> mRenderFinishedSemaphores;
        std::vector<VkSemaphore> mPresentFinishedSemaphores;
        std::vector<VkFence> mInFlightFences;

        uint32_t mInFlightIndex = 0;
        uint32_t mPreviousInFlightIndex = 0;
        uint32_t mImageIndex = 0;

        VkDescriptorSetLayout mDescriptorSetLayout;
        VkDescriptorPool mDescriptorPool;
        std::vector<VkDescriptorSet> mDescriptorSets;

        bool mRecreated = false;

        enum class ScreenshotState {
            Idle,
            Requested,
            QueuedForBlitting,
            BlittedToStage,
            CopiedToHost,
        };

        ScreenshotState mScreenshotState = ScreenshotState::Idle;
        ptr<Image> mScreenshotStageImage;
        std::mutex mScreenshotMutex;
        std::condition_variable mScreenshotAvailableCV;
        uint32_t mScreenshotInFlightIndex;
    };
} // namespace Mandrill
