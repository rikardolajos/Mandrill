#pragma once

#include "Common.h"

#include "Device.h"
#include "Log.h"

namespace Mandrill
{
    class Image
    {
    public:
        MANDRILL_NON_COPYABLE(Image)

        /// <summary>
        /// Create a new Image and allocate memory for it.
        /// </summary>
        /// <param name="pDevice">Device pointer</param>
        /// <param name="width">Width of image</param>
        /// <param name="height">Height of image</param>
        /// <param name="depth">Depth of image</param>
        /// <param name="mipLevels">Number of mipmapping levels</param>
        /// <param name="samples">Number of samples</param>
        /// <param name="format">Image format</param>
        /// <param name="tiling">Tiling mode to use</param>
        /// <param name="usage">How the image will be used</param>
        /// <param name="properties">Which memory properties to require</param>
        /// <returns>Image object</returns>
        MANDRILL_API Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
                           VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

        /// <summary>
        /// Create a new Image using memory that has already been allocated.
        /// </summary>
        /// <param name="pDevice">Device pointer</param>
        /// <param name="width">Width of image</param>
        /// <param name="height">Height of image</param>
        /// <param name="depth">Depth of image</param>
        /// <param name="mipLevels">Number of mipmapping levels</param>
        /// <param name="samples">Number of samples</param>
        /// <param name="format">Image format</param>
        /// <param name="tiling">Tiling mode to use</param>
        /// <param name="usage">How the image will be used</param>
        /// <param name="memory">Allocated memory to use for image</param>
        /// <param name="offset">Where in the allocated memory the image should be stored</param>
        /// <returns>Image object</returns>
        MANDRILL_API Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
                           VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkDeviceMemory memory, VkDeviceSize offset);

        /// <summary>
        /// Destructor for image
        /// </summary>
        /// <returns></returns>
        MANDRILL_API ~Image();

        /// <summary>
        /// Create a default image view for Image object.
        /// </summary>
        /// <param name="aspectFlags">Aspect flags to use for image view</param>
        /// <returns></returns>
        MANDRILL_API void createImageView(VkImageAspectFlags aspectFlags);

        /// <summary>
        /// Get the VkImage handle.
        /// </summary>
        /// <returns>VkImage handle</returns>
        MANDRILL_API VkImage getImage() const
        {
            return mImage;
        }

        /// <summary>
        /// Use this function if image view is created externally.
        /// </summary>
        /// <param name="imageView">Image view to use</param>
        /// <returns></returns>
        MANDRILL_API void setImageView(VkImageView imageView)
        {
            mImageView = imageView;
        }

        /// <summary>
        /// Get the VkImageView handle.
        /// </summary>
        /// <returns>VkImageView handle</returns>
        MANDRILL_API VkImageView getImageView() const
        {
            return mImageView;
        }

        /// <summary>
        /// Get the device memory.
        /// </summary>
        /// <returns>VkDeviceMemory handle</returns>
        MANDRILL_API VkDeviceMemory getMemory() const
        {
            return mMemory;
        }

        /// <summary>
        /// Get the image usage flags.
        /// </summary>
        /// <returns>Usage flags</returns>
        MANDRILL_API VkImageUsageFlags getUsage() const
        {
            return mUsage;
        }

        /// <summary>
        /// Get the memory property flags
        /// </summary>
        /// <returns>Memory property flags</returns>
        MANDRILL_API VkMemoryPropertyFlags getProperties() const
        {
            return mProperties;
        }

        /// <summary>
        /// If the buffer memory is host-coherent, this returns the pointor to the memory.
        /// </summary>
        /// <returns>Pointer to buffer memory, or nullptr</returns>
        MANDRILL_API void* getHostMap() const
        {
            if (!(mProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                Log::Error("Unable to access host map of buffer that is not host coherent.");
                return nullptr;
            }
            return mpHostMap;
        }

        /// <summary>
        /// Get the format of the image.
        /// </summary>
        /// <returns>Image format</returns>
        MANDRILL_API VkFormat getFormat() const
        {
            return mFormat;
        }

        /// <summary>
        /// Get the width of the image.
        /// </summary>
        /// <returns>Image width</returns>
        MANDRILL_API uint32_t getWidth() const
        {
            return mWidth;
        }

        /// <summary>
        /// Get the height of the image.
        /// </summary>
        /// <returns>Image height</returns>
        MANDRILL_API uint32_t getHeight() const
        {
            return mHeight;
        }

        /// <summary>
        /// Get the depth of the image.
        /// </summary>
        /// <returns>Image height</returns>
        MANDRILL_API uint32_t getDepth() const
        {
            return mDepth;
        }

        /// <summary>
        /// Get the mipmap levels of the image.
        /// </summary>
        /// <returns>Image mipmap levels</returns>
        MANDRILL_API uint32_t getMipLevels() const
        {
            return mMipLevels;
        }

    private:
        ptr<Device> mpDevice;

        VkImage mImage;
        VkImageView mImageView;

        VkImageUsageFlags mUsage;
        VkMemoryPropertyFlags mProperties;

        VkDeviceMemory mMemory;
        bool mOwnMemory;
        void* mpHostMap;

        uint32_t mWidth;
        uint32_t mHeight;
        uint32_t mDepth;

        uint32_t mMipLevels;
        VkFormat mFormat;
        VkImageTiling mTiling;
    };
} // namespace Mandrill
