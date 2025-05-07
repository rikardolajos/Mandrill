#include "Texture.h"

#include "Buffer.h"
#include "Error.h"
#include "Helpers.h"
#include "Log.h"

#include <stb_image.h>

using namespace Mandrill;

Texture::Texture(ptr<Device> pDevice, Type type, VkFormat format, const std::filesystem::path& path, bool mipmaps)
    : mpDevice(pDevice), mImageInfo{0}
{
    std::filesystem::path fullPath = path;
    if (path.is_relative()) {
        fullPath = getExecutablePath() / path;
    }

    Log::Info("Loading texture from {}", path.string());

    switch (type) {
    case Type::Texture1D: {
        Log::Error("Texture1D cannot be read from file");
        break;
    }
    case Type::Texture2D: {
        stbi_set_flip_vertically_on_load(1);

        std::string pathStr = fullPath.string();
        int width, height, channels;
        stbi_uc* pData = stbi_load(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        channels = STBI_rgb_alpha;

        if (!pData) {
            Log::Error("Failed to load texture.");
            return;
        }

        create(format, pData, width, height, 1, channels, mipmaps);

        stbi_image_free(pData);
        break;
    }
    case Type::Texture3D: {
        // TODO: Load using OpenVDB?
        break;
    }
    case Type::CubeMap: {
        Log::Error("Not implemented");
        break;
    }
    }
}

Texture::Texture(ptr<Device> pDevice, Type type, VkFormat format, const void* pData, uint32_t width, uint32_t height,
                 uint32_t depth, uint32_t channels, bool mipmaps)
    : mpDevice(pDevice), mImageInfo{0}
{
    create(format, pData, width, height, depth, channels, mipmaps);
}

Texture::~Texture()
{
}

void Texture::create(VkFormat format, const void* pData, uint32_t width, uint32_t height, uint32_t depth,
                     uint32_t channels, bool mipmaps)
{
    uint32_t mipLevels = 1;
    if (mipmaps) {
        mipLevels = static_cast<uint32_t>(std::floor(log2(std::max(width, height))) + 1);
    }

    mpImage = make_ptr<Image>(
        mpDevice, width, height, depth, mipLevels, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (pData) {
        VkDeviceSize size = width * height * channels;

        Buffer staging(mpDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        staging.copyFromHost(pData, size);

        Helpers::transitionImageLayout(mpDevice, mpImage->getImage(), format, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);

        Helpers::copyBufferToImage(mpDevice, staging.getBuffer(), mpImage->getImage(), static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height));

        if (mipmaps) {
            generateMipmaps();
        } else {
            Helpers::transitionImageLayout(mpDevice, mpImage->getImage(), format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);
        }
    }

    mpImage->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);

    mImageInfo = {
        .sampler = nullptr,
        .imageView = mpImage->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
}

void Texture::generateMipmaps()
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(mpDevice->getPhysicalDevice(), mpImage->getFormat(), &props);

    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        Log::Error("Texture image format does not support linear blitting");
    }

    VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);

    VkImageSubresourceRange subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    int32_t mipWidth = mpImage->getWidth();
    int32_t mipHeight = mpImage->getHeight();

    for (uint32_t i = 1; i < mpImage->getMipLevels(); i++) {
        subresourceRange.baseMipLevel = i - 1;

        Helpers::imageBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              mpImage->getImage(), &subresourceRange);

        VkImageBlit2 region = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .mipLevel = i - 1,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
            .srcOffsets = {{0, 0, 0}, {mipWidth, mipHeight, 1}},
            .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .mipLevel = i,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
            .dstOffsets = {{0, 0, 0}, {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}},
        };

        VkBlitImageInfo2 blitImageInfo = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .srcImage = mpImage->getImage(),
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = mpImage->getImage(),
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &region,
            .filter = VK_FILTER_NEAREST,
        };

        vkCmdBlitImage2(cmd, &blitImageInfo);

        Helpers::imageBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT,
                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              mpImage->getImage(), &subresourceRange);

        if (mipWidth > 1) {
            mipWidth /= 2;
        }
        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    subresourceRange.baseMipLevel = mpImage->getMipLevels() - 1;

    Helpers::imageBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          mpImage->getImage(), &subresourceRange);

    Helpers::cmdEnd(mpDevice, cmd);
}
