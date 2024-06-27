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

        MANDRILL_API Texture(std::shared_ptr<Device> pDevice, Type type, VkFormat format,
                             const std::filesystem::path& path, bool mipmaps = false);
        MANDRILL_API ~Texture();

        MANDRILL_API void setSampler(const std::shared_ptr<Sampler> sampler);

        MANDRILL_API VkWriteDescriptorSet getDescriptor(uint32_t binding) const;

    private:
        void generateMipmaps();

        std::shared_ptr<Device> mpDevice;

        int mWidth, mHeight, mChannels;
        uint32_t mMipLevels;
        VkFormat mFormat;
        std::shared_ptr<Image> mpImage;
        VkDescriptorImageInfo mImageInfo;
    };
} // namespace Mandrill