#pragma once

#include "Common.h"

#include "Device.h"

namespace Mandrill
{
    class Image
    {
    public:
        /// <summary>
        /// Create a new Image and allocate memory for it.
        /// </summary>
        /// <param name="pDevice">Device pointer</param>
        /// <param name="width">Width of image</param>
        /// <param name="height">Height of image</param>
        /// <param name="mipLevels">Number of mipmapping levels</param>
        /// <param name="samples">Number of samples</param>
        /// <param name="format">Image format</param>
        /// <param name="tiling">Tiling mode to use</param>
        /// <param name="usage">How the image will be used</param>
        /// <param name="properties">Which memory properties to require</param>
        /// <returns>Image object</returns>
        MANDRILL_API Image(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t mipLevels,
                           VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

        /// <summary>
        /// Create a new Image using memory that has already been allocated.
        /// </summary>
        /// <param name="pDevice">Device pointer</param>
        /// <param name="width">Width of image</param>
        /// <param name="height">Height of image</param>
        /// <param name="mipLevels">Number of mipmapping levels</param>
        /// <param name="samples">Number of samples</param>
        /// <param name="format">Image format</param>
        /// <param name="tiling">Tiling mode to use</param>
        /// <param name="usage">How the image will be used</param>
        /// <param name="memory">Allocated memory to use for image</param>
        /// <param name="offset">Where in the allocated memory the image should be stored</param>
        /// <returns>Image object</returns>
        MANDRILL_API Image(std::shared_ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t mipLevels,
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
        /// Get the VkImageView handle.
        /// </summary>
        /// <returns>VkImageView handle</returns>
        MANDRILL_API VkImageView getImageView() const
        {
            return mImageView;
        }

        /// <summary>
        /// Get the format of the image.
        /// </summary>
        /// <returns>Image format</returns>
        MANDRILL_API VkFormat getFormat() const
        {
            return mFormat;
        }

    private:
        std::shared_ptr<Device> mpDevice;

        VkImage mImage;
        VkImageView mImageView;

        VkDeviceMemory mMemory;
        bool mOwnMemory;

        uint32_t mMipLevels;
        VkFormat mFormat;
        VkImageTiling mTiling;
    };
} // namespace Mandrill
