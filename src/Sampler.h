#pragma once

#include "Common.h"

#include "Device.h"

namespace Mandrill
{
    /// <summary>
    /// Sampler class for managing texture samplers in Vulkan.
    /// </summary>
    class Sampler
    {
    public:
        MANDRILL_NON_COPYABLE(Sampler)

        /// <summary>
        /// Create a new texture sampler.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="magFilter">Magnification filter</param>
        /// <param name="minFilter">Minification filter</param>
        /// <param name="mipmapMode">MIP map mode</param>
        /// <param name="addressModeU">Address mode/wrapping mode U</param>
        /// <param name="addressModeV">Address mode/wrapping mode V</param>
        /// <param name="addressModeW">Address mode/wrapping mode W</param>
        MANDRILL_API Sampler(ptr<Device> pDevice, VkFilter magFilter = VK_FILTER_LINEAR,
                             VkFilter minFilter = VK_FILTER_LINEAR,
                             VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                             VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                             VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT);
        
        /// <summary>
        /// Destructor for sampler.
        /// </summary>
        MANDRILL_API ~Sampler();

        /// <summary>
        /// Get the sampler handle.
        /// </summary>
        /// <returns>Sampler handle</returns>
        MANDRILL_API VkSampler getSampler() const
        {
            return mSampler;
        }

    private:
        ptr<Device> mpDevice;

        VkSampler mSampler;
    };
} // namespace Mandrill
