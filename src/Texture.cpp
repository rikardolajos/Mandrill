#include "Texture.h"

#include "Buffer.h"
#include "Error.h"
#include "Helpers.h"
#include "Log.h"

#include <stb_image.h>

using namespace Mandrill;

Texture::Texture(ptr<Device> pDevice, TextureType type, VkFormat format, const std::filesystem::path& path,
                 bool mipmaps)
    : mpDevice(pDevice), mImageInfo{0}
{
    std::filesystem::path fullPath = path;
    if (path.is_relative()) {
        fullPath = GetExecutablePath() / path;
    }

    Log::Info("Loading texture from {}", path.string());

    switch (type) {
    case TextureType::Texture1D: {
        Log::Error("Texture1D cannot be read from file");
        break;
    }
    case TextureType::Texture2D: {
        stbi_set_flip_vertically_on_load(1);

        std::string pathStr = fullPath.string();
        int width, height, channels;
        stbi_uc* pData = stbi_load(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        channels = STBI_rgb_alpha;

        if (!pData) {
            Log::Error("Failed to load texture");
            return;
        }

        create(format, pData, width, height, 1, sizeof(stbi_uc) * channels, mipmaps);

        stbi_image_free(pData);
        break;
    }
    case TextureType::Texture3D: {
#ifdef MANDRILL_USE_OPENVDB
        openvdb::io::File file(path.string());
        file.open();

        Log::Debug("Grids in volume:");
        for (auto iter = file.beginName(); iter != file.endName(); ++iter) {
            Log::Debug("\t{}", *iter);
        }

        auto found = std::find_if(file.beginName(), file.endName(), [](const auto& grid) { return grid == "density"; });
        if (found == file.endName()) {
            Log::Error("Density grid not found in volume");
        }

        auto baseGrid = file.readGrid("density");
        auto floatGrid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);

        openvdb::CoordBBox bbox = floatGrid->evalActiveVoxelBoundingBox();
        int32_t sizeX = bbox.dim().x();
        int32_t sizeY = bbox.dim().y();
        int32_t sizeZ = bbox.dim().z();

        std::vector<float> volumeData(sizeX * sizeY * sizeZ);

        auto accessor = floatGrid->getAccessor();
        for (int z = bbox.min().z(); z <= bbox.max().z(); ++z) {
            for (int y = bbox.min().y(); y <= bbox.max().y(); ++y) {
                for (int x = bbox.min().x(); x <= bbox.max().x(); ++x) {
                    openvdb::Coord ijk(x, y, z);
                    float density = accessor.getValue(ijk);
                    int32_t localX = x - bbox.min().x();
                    int32_t localY = y - bbox.min().y();
                    int32_t localZ = z - bbox.min().z();
                    int32_t idx = (localZ * sizeY + localY) * sizeX + localX;
                    volumeData[idx] = density;
                }
            }
        }

        create(format, volumeData.data(), sizeX, sizeY, sizeZ, sizeof(float), false);
#else
        Log::Error("Trying to load a Texture3D but Mandrill was not compiled with OpenVDB support. OpenVDB can be "
                   "installed using vcpkg:\n\t`vcpkg install openvdb`");
#endif
        break;
    }
    case TextureType::CubeMap: {
        Log::Error("Not implemented");
        break;
    }
    }
    createSampler();
}

Texture::Texture(ptr<Device> pDevice, TextureType type, VkFormat format, const void* pData, uint32_t width,
                 uint32_t height, uint32_t depth, uint32_t channels, bool mipmaps)
    : mpDevice(pDevice), mImageInfo{0}
{
    create(format, pData, width, height, depth, channels, mipmaps);
    createSampler();
}

Texture::~Texture()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    vkDestroySampler(mpDevice->getDevice(), mSampler, nullptr);
}

void Texture::create(VkFormat format, const void* pData, uint32_t width, uint32_t height, uint32_t depth,
                     uint32_t bytesPerPixel, bool mipmaps)
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
        VkDeviceSize size = width * height * depth * bytesPerPixel;

        Buffer staging(mpDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        staging.copyFromHost(pData, size);

        VkImageSubresourceRange subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = mipLevels,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);

        Helpers::imageBarrier(cmd, mpImage->getImage(), VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                              VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &subresourceRange);

        Helpers::copyBufferToImage(cmd, staging.getBuffer(), mpImage->getImage(), width, height, depth);

        if (mipmaps) {
            generateMipmaps(cmd);
        } else {
            Helpers::imageBarrier(cmd, mpImage->getImage(), VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                  VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &subresourceRange);
        }

        Helpers::cmdEnd(mpDevice, cmd);
    }

    mpImage->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);

    mImageInfo = {
        .sampler = nullptr,
        .imageView = mpImage->getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
}

void Texture::generateMipmaps(VkCommandBuffer cmd)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(mpDevice->getPhysicalDevice(), mpImage->getFormat(), &props);

    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        Log::Error("Texture image format does not support linear blitting");
    }

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

        Helpers::imageBarrier(cmd, mpImage->getImage(), VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                              VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                              VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &subresourceRange);

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
            .filter = VK_FILTER_LINEAR,
        };

        vkCmdBlitImage2(cmd, &blitImageInfo);

        Helpers::imageBarrier(cmd, mpImage->getImage(), VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                              VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &subresourceRange);

        if (mipWidth > 1) {
            mipWidth /= 2;
        }
        if (mipHeight > 1) {
            mipHeight /= 2;
        }
    }

    subresourceRange.baseMipLevel = mpImage->getMipLevels() - 1;

    Helpers::imageBarrier(cmd, mpImage->getImage(), VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &subresourceRange);
}

void Texture::createSampler()
{
    if (mSampler != VK_NULL_HANDLE) {
        vkDestroySampler(mpDevice->getDevice(), mSampler, nullptr);
    }

    VkSamplerCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = mMagFilter,
        .minFilter = mMinFilter,
        .mipmapMode = mMipmapMode,
        .addressModeU = mAddressModeU,
        .addressModeV = mAddressModeV,
        .addressModeW = mAddressModeW,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = mpDevice->getProperties().physicalDevice.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = VK_LOD_CLAMP_NONE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    Check::Vk(vkCreateSampler(mpDevice->getDevice(), &ci, nullptr, &mSampler));

    mImageInfo.sampler = mSampler;
}