#pragma once

#include "Common.h"

#include "Device.h"
#include "Image.h"
#include "Sampler.h"

namespace Mandrill
{
    class Texture
    {
    public:
        enum class MANDRILL_API Type {
            Texture1D,
            Texture2D,
            Texture3D,
            CubeMap,
        };

        MANDRILL_API Texture(ptr<Device> pDevice, Type type, VkFormat format, const std::filesystem::path& path,
                             bool mipmaps = false);
        MANDRILL_API ~Texture();

        MANDRILL_API void setSampler(const ptr<Sampler> pSampler)
        {
            mImageInfo.sampler = pSampler->getSampler();
        }

        MANDRILL_API VkSampler getSampler()
        {
            return mImageInfo.sampler;
        }

        MANDRILL_API VkImageView getImageView()
        {
            return mImageInfo.imageView;
        }

    private:
        void generateMipmaps();

        ptr<Device> mpDevice;

        int mWidth, mHeight, mChannels;
        uint32_t mMipLevels;
        VkFormat mFormat;
        ptr<Image> mpImage;
        VkDescriptorImageInfo mImageInfo;
    };
} // namespace Mandrill