#pragma once

#include "Common.h"

#include "Device.h"

namespace Mandrill
{
    class Sampler
    {
    public:
        MANDRILL_API Sampler(ptr<Device> pDevice, VkFilter magFilter = VK_FILTER_LINEAR,
                             VkFilter minFilter = VK_FILTER_LINEAR,
                             VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                             VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT);
        MANDRILL_API ~Sampler();

        MANDRILL_API VkSampler getSampler() const
        {
            return mSampler;
        }

    private:
        ptr<Device> mpDevice;

        VkSampler mSampler;
    };
} // namespace Mandrill