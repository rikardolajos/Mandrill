#pragma once

#include "Common.h"

#include "Device.h"
#include "Log.h"

namespace Mandrill
{
    /// <summary>
    /// Image class for managing Vulkan images. This class handles the creation and destruction of images, as well as
    /// basic memory management.
    /// </summary>
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
        /// <param name="type">Image type. Defaults to deriving it from the extent, which cannot tell a 1 x 1 2D
        /// image from a 1D one, so pass it explicitly when the dimensionality matters</param>
        MANDRILL_API Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
                           VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                           VkImageType type = VK_IMAGE_TYPE_MAX_ENUM);

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
        /// <param name="type">Image type. Defaults to deriving it from the extent, which cannot tell a 1 x 1 2D
        /// image from a 1D one, so pass it explicitly when the dimensionality matters</param>
        MANDRILL_API Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
                           VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkDeviceMemory memory, VkDeviceSize offset,
                           VkImageType type = VK_IMAGE_TYPE_MAX_ENUM);

        /// <summary>
        /// Destructor for image
        /// </summary>
        MANDRILL_API ~Image();

        /// <summary>
        /// Create a default image view for Image object.
        /// </summary>
        /// <param name="aspectFlags">Aspect flags to use for image view</param>
        /// <param name="viewType">View type. Defaults to matching the image type, which is what a shader sampling
        /// the image expects</param>
        MANDRILL_API void createImageView(VkImageAspectFlags aspectFlags,
                                          VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM);

        /// <summary>
        /// Save the image to a PNG file. The image is blitted to a host-visible staging image first, so it has to be
        /// single-sampled, created with VK_IMAGE_USAGE_TRANSFER_SRC_BIT, and left in
        /// VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, which is where Pass::end() leaves the output of a pass. This is how
        /// a headless application gets its rendering out, in place of the screenshots App takes from the swapchain.
        /// </summary>
        /// <param name="path">File to write to</param>
        MANDRILL_API void saveToPNG(const std::filesystem::path& path) const;

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
        /// Get the number of bytes between each row of the image. Only valid for linear tiled images.
        /// </summary>
        /// <returns>Image pitch</returns>
        MANDRILL_API uint32_t getPitch() const
        {
            return mPitch;
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
        uint32_t mPitch;

        uint32_t mMipLevels;
        VkFormat mFormat;
        VkImageTiling mTiling;
        VkImageType mType;
    };
} // namespace Mandrill
