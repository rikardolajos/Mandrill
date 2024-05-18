#pragma once

#include "Common.h"

namespace Mandrill
{
    class MANDRILL_API Image
    {
    public:
        Image(VkDevice device, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits samples,
              VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);
        ~Image();

        void createImageView(VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

        VkDevice mDevice;
        VkImage mImage;
        VkImageView mImageView;
    };
} // namespace Mandrill
