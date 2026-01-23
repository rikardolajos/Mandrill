#pragma once

#include "Common.h"

#include "Device.h"
#include "Image.h"

namespace Mandrill
{
    enum class MANDRILL_API TextureType : uint32_t {
        Texture1D,
        Texture2D,
        Texture3D,
        CubeMap,
    };

    /// <summary>
    /// Texture class for managing textures in Vulkan.
    /// </summary>
    class Texture
    {
    public:
        MANDRILL_NON_COPYABLE(Texture)

        /// <summary>
        /// Create a new texture from a file.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="type">Type of texture</param>
        /// <param name="format">Format to use</param>
        /// <param name="path">Path to texture file</param>
        /// <param name="mipmaps">Whether to use mipmaps or not</param>
        MANDRILL_API Texture(ptr<Device> pDevice, TextureType type, VkFormat format, const std::filesystem::path& path,
                             bool mipmaps = false);

        /// <summary>
        /// Create a new texture from a buffer.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="type">Type of texture</param>
        /// <param name="format">Format to use</param>
        /// <param name="pData">Pointer to texture data</param>
        /// <param name="width">Width of texture</param>
        /// <param name="height">Height of texture</param>
        /// <param name="depth">Depth of texture</param>
        /// <param name="bytesPerPixel">Number of bytes per pixel</param>
        /// <param name="mipmaps">Whether to use mipmaps or not</param>
        MANDRILL_API Texture(ptr<Device> pDevice, TextureType type, VkFormat format, const void* pData, uint32_t width,
                             uint32_t height, uint32_t depth, uint32_t bytesPerPixel, bool mipmaps = false);

        /// <summary>
        /// Create a texture from an existing image.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="pImage">Image to use</param>
        /// <param name="mipmaps">Whether to use mipmaps or not</param>
        /// <returns></returns>
        MANDRILL_API Texture(ptr<Device> pDevice, ptr<Image> pImage, bool mipmaps = false);

        /// <summary>
        /// Destructor for texture.
        /// </summary>
        MANDRILL_API ~Texture();

        /// <summary>
        /// Get the sampler handle currently in use by the texture.
        /// </summary>
        /// <returns>Sampler in use</returns>
        MANDRILL_API VkSampler getSampler() const
        {
            return mSampler;
        }

        /// <summary>
        /// Get the image of the texture.
        /// </summary>
        /// <returns>Pointer to image</returns>
        MANDRILL_API ptr<Image> getImage() const
        {
            return mpImage;
        }

        /// <summary>
        /// Get the image view handle.
        /// </summary>
        /// <returns>Image view handle</returns>
        MANDRILL_API VkImageView getImageView() const
        {
            return mImageInfo.imageView;
        }

        /// <summary>
        /// Get the write descriptor set. Useful when using push descriptors.
        /// </summary>
        /// <param name="binding">Binding to place the write descriptor in</param>
        /// <returns>Write descriptor set</returns>
        MANDRILL_API VkWriteDescriptorSet getWriteDescriptor(uint32_t binding) const
        {
            VkWriteDescriptorSet descriptor = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = binding,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &mImageInfo,
            };

            return descriptor;
        }

        /// <summary>
        /// Set the magnification filter.
        /// </summary>
        /// <param name="filter">Magnification filter</param>
        /// <returns></returns>
        MANDRILL_API void setMagFilter(VkFilter filter)
        {
            mMagFilter = filter;
            createSampler();
        }

        /// <summary>
        /// Set the minification filter.
        /// </summary>
        /// <param name="filter">Minification filter</param>
        /// <returns></returns>
        MANDRILL_API void setMinFilter(VkFilter filter)
        {
            mMinFilter = filter;
            createSampler();
        }

        /// <summary>
        /// Set the mipmap mode.
        /// </summary>
        /// <param name="mode">Mipmap mode</param>
        /// <returns></returns>
        MANDRILL_API void setMipmapMode(VkSamplerMipmapMode mode)
        {
            mMipmapMode = mode;
            createSampler();
        }

        /// <summary>
        /// Set the address mode for U, V and W coordinates.
        /// </summary>
        /// <param name="modeU">U address mode</param>
        /// <param name="modeV">V address mode</param>
        /// <param name="modeW">W address mode</param>
        /// <returns></returns>
        MANDRILL_API void setAddressMode(VkSamplerAddressMode modeU, VkSamplerAddressMode modeV,
                                         VkSamplerAddressMode modeW)
        {
            mAddressModeU = modeU;
            mAddressModeV = modeV;
            mAddressModeW = modeW;
            createSampler();
        }

    private:
        void create(VkFormat format, const void* pData, uint32_t width, uint32_t height, uint32_t depth,
                    uint32_t bytesPerPixel, bool mipmaps);
        void generateMipmaps(VkCommandBuffer cmd);
        void createSampler();

        ptr<Device> mpDevice;

        ptr<Image> mpImage;
        VkDescriptorImageInfo mImageInfo;

        VkSampler mSampler = VK_NULL_HANDLE;
        VkFilter mMagFilter = VK_FILTER_LINEAR;
        VkFilter mMinFilter = VK_FILTER_LINEAR;
        VkSamplerMipmapMode mMipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkSamplerAddressMode mAddressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode mAddressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode mAddressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    };
} // namespace Mandrill
