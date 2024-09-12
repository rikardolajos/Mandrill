#include "Texture.h"

#include "Buffer.h"
#include "Error.h"
#include "Helpers.h"
#include "Log.h"

#include <stb_image.h>

using namespace Mandrill;


Texture::Texture(std::shared_ptr<Device> pDevice, Type type, VkFormat format, const std::filesystem::path& path,
                 bool mipmaps)
    : mpDevice(pDevice), mFormat(format), mMipLevels(1), mImageInfo{0}
{
    Log::info("Loading texture from {}", path.string());

    stbi_set_flip_vertically_on_load(1);

    std::string pathStr = path.string();
    stbi_uc* pixels = stbi_load(pathStr.c_str(), &mWidth, &mHeight, &mChannels, STBI_rgb_alpha);

    if (!pixels) {
        Log::error("Failed to load texture.");
        return;
    }

    if (mipmaps) {
        mMipLevels = static_cast<uint32_t>(std::floor(log2(std::max(mWidth, mHeight))) + 1);
    }

    VkDeviceSize size = mWidth * mHeight * STBI_rgb_alpha;

    Buffer staging(mpDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    staging.copyFromHost(pixels, size);

    stbi_image_free(pixels);

    mpImage = std::make_shared<Image>(
        mpDevice, mWidth, mHeight, mMipLevels, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    Helpers::transitionImageLayout(mpDevice, mpImage->getImage(), mFormat, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mMipLevels);

    Helpers::copyBufferToImage(mpDevice, staging, *mpImage, static_cast<uint32_t>(mWidth),
                               static_cast<uint32_t>(mHeight));

    if (mipmaps) {
        generateMipmaps();
    } else {
        Helpers::transitionImageLayout(mpDevice, mpImage->getImage(), mFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mMipLevels);
    }

    mpImage->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);

    mImageInfo = {
        .sampler = nullptr,
        .imageView = mpImage->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
}

Texture::~Texture()
{
}

void Texture::generateMipmaps()
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(mpDevice->getPhysicalDevice(), mFormat, &props);

    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        Log::error("Texture image format does not support linear blitting");
    }

    VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = mpImage->getImage(),
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    int32_t mipWidth = mWidth;
    int32_t mipHeight = mHeight;

    for (uint32_t i = 1; i < mMipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &barrier);

        VkImageBlit blit = {
            .srcSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = i - 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .srcOffsets = {{0, 0, 0}, {mipWidth, mipHeight, 1}},
            .dstSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = i,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .dstOffsets = {{0, 0, 0}, {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}},
        };

        vkCmdBlitImage(cmd, mpImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mpImage->getImage(),
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &barrier);

        if (mipWidth > 1) {
            mipWidth /= 2;
        }
        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = mMipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    Helpers::cmdEnd(mpDevice, cmd);
}
