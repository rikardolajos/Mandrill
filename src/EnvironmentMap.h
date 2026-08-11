#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "Texture.h"

namespace Mandrill
{
    /// <summary>
    /// Environment map stored as a lat-long texture, together with a distribution for importance sampling it.
    ///
    /// A renderer that samples light directions uniformly will rarely hit a small bright region such as a sun, and the
    /// few paths that do hit it carry an enormous weight. That shows up as fireflies. The distribution built here lets
    /// directions be sampled proportionally to the radiance arriving from them instead, which removes that variance.
    ///
    /// The distribution is piecewise-constant over the texels, weighted by luminance and by the sine of the polar
    /// angle (the latter because rows near the poles cover less solid angle). It is exposed as two storage buffers
    /// holding cumulative distribution functions, both normalized to [0, 1]:
    /// <list type="bullet">
    /// <item>A marginal CDF over the rows, holding height + 1 values.</item>
    /// <item>A conditional CDF over the texels within each row, holding height * (width + 1) values, where the CDF of
    /// row j starts at index j * (width + 1).</item>
    /// </list>
    /// </summary>
    class EnvironmentMap
    {
    public:
        MANDRILL_NON_COPYABLE(EnvironmentMap)

        /// <summary>
        /// Create an environment map from a file. Use a floating-point format to keep the dynamic range of HDR files,
        /// which is what makes importance sampling worthwhile in the first place.
        ///
        /// Note that the file is read twice, once for the texture and once for the sampling distribution.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="path">Path to environment map file</param>
        /// <param name="format">Format to use for the texture</param>
        MANDRILL_API EnvironmentMap(ptr<Device> pDevice, const std::filesystem::path& path,
                                    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT);

        /// <summary>
        /// Create a uniform white environment map of a single texel. Useful as a placeholder before a map has been
        /// loaded, so that descriptors can be created and bound regardless.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        MANDRILL_API EnvironmentMap(ptr<Device> pDevice);

        /// <summary>
        /// Destructor for environment map.
        /// </summary>
        MANDRILL_API ~EnvironmentMap();

        /// <summary>
        /// Get the environment map texture.
        /// </summary>
        /// <returns>Pointer to texture</returns>
        MANDRILL_API ptr<Texture> getTexture() const
        {
            return mpTexture;
        }

        /// <summary>
        /// Get the buffer holding the marginal CDF over the rows.
        /// </summary>
        /// <returns>Pointer to buffer</returns>
        MANDRILL_API ptr<Buffer> getMarginalDistribution() const
        {
            return mpMarginal;
        }

        /// <summary>
        /// Get the buffer holding the conditional CDF over the texels of each row.
        /// </summary>
        /// <returns>Pointer to buffer</returns>
        MANDRILL_API ptr<Buffer> getConditionalDistribution() const
        {
            return mpConditional;
        }

        /// <summary>
        /// Get the width of the environment map.
        /// </summary>
        /// <returns>Width in texels</returns>
        MANDRILL_API uint32_t getWidth() const
        {
            return mWidth;
        }

        /// <summary>
        /// Get the height of the environment map.
        /// </summary>
        /// <returns>Height in texels</returns>
        MANDRILL_API uint32_t getHeight() const
        {
            return mHeight;
        }

    private:
        void createDistribution(const float* pData, uint32_t width, uint32_t height);

        ptr<Device> mpDevice;

        ptr<Texture> mpTexture;
        ptr<Buffer> mpMarginal;
        ptr<Buffer> mpConditional;

        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
    };
} // namespace Mandrill
