#include "Image.h"

#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

namespace
{
    // Falls back to guessing from the extent, which is ambiguous for images that are one texel tall
    VkImageType resolveImageType(VkImageType requested, uint32_t height, uint32_t depth)
    {
        if (requested != VK_IMAGE_TYPE_MAX_ENUM) {
            return requested;
        }
        return height == 1 ? VK_IMAGE_TYPE_1D : (depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D);
    }
} // namespace

Image::Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
             VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
             VkMemoryPropertyFlags properties, VkImageType type)
    : mpDevice(pDevice), mWidth(width), mHeight(height), mDepth(depth), mPitch(0), mMipLevels(mipLevels),
      mFormat(format), mTiling(tiling), mUsage(usage), mProperties(properties), mImageView(VK_NULL_HANDLE),
      mOwnMemory(true), mpHostMap(nullptr), mType(resolveImageType(type, height, depth))
{
    VkImageCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = mType,
        .format = mFormat,
        .extent = {.width = mWidth, .height = mHeight, .depth = mDepth},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = mTiling,
        .usage = mUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    Check::Vk(vkCreateImage(mpDevice->getDevice(), &ci, nullptr, &mImage));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(mpDevice->getDevice(), mImage, &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = Helpers::findMemoryType(mpDevice, memReqs.memoryTypeBits, mProperties),
    };

    Check::Vk(vkAllocateMemory(mpDevice->getDevice(), &allocInfo, nullptr, &mMemory));

    Check::Vk(vkBindImageMemory(mpDevice->getDevice(), mImage, mMemory, 0));

    // Map memory if it is host coherent
    if (mProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        Check::Vk(vkMapMemory(mpDevice->getDevice(), mMemory, 0, memReqs.size, 0, &mpHostMap));
    }

    if (mTiling == VK_IMAGE_TILING_LINEAR) {
        VkImageSubresource subresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .arrayLayer = 0,
        };
        VkSubresourceLayout layout;
        vkGetImageSubresourceLayout(mpDevice->getDevice(), mImage, &subresource, &layout);
        mPitch = static_cast<uint32_t>(layout.rowPitch);
    }
}

Image::Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
             VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
             VkDeviceMemory memory, VkDeviceSize offset, VkImageType type)
    : mpDevice(pDevice), mWidth(width), mHeight(height), mDepth(depth), mPitch(0), mMipLevels(mipLevels),
      mFormat(format), mTiling(tiling), mUsage(usage), mProperties(0), mImageView(VK_NULL_HANDLE), mMemory(memory),
      mOwnMemory(false), mpHostMap(nullptr), mType(resolveImageType(type, height, depth))
{
    VkImageCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = mType,
        .format = format,
        .extent = {.width = mWidth, .height = mHeight, .depth = mDepth},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = mTiling,
        .usage = mUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    Check::Vk(vkCreateImage(mpDevice->getDevice(), &ci, nullptr, &mImage));
    Check::Vk(vkBindImageMemory(mpDevice->getDevice(), mImage, mMemory, offset));

    if (mTiling == VK_IMAGE_TILING_LINEAR) {
        VkImageSubresource subresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .arrayLayer = 0,
        };
        VkSubresourceLayout layout;
        vkGetImageSubresourceLayout(mpDevice->getDevice(), mImage, &subresource, &layout);
        mPitch = static_cast<uint32_t>(layout.rowPitch);
    }
}

Image::~Image()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    if (mOwnMemory) {
        if (mpHostMap) {
            vkUnmapMemory(mpDevice->getDevice(), mMemory);
            mpHostMap = nullptr;
        }
        vkFreeMemory(mpDevice->getDevice(), mMemory, nullptr);
    }

    if (mImageView) {
        vkDestroyImageView(mpDevice->getDevice(), mImageView, nullptr);
    }

    vkDestroyImage(mpDevice->getDevice(), mImage, nullptr);
}

void Image::createImageView(VkImageAspectFlags aspectFlags, VkImageViewType viewType)
{
    if (mImageView) {
        vkDeviceWaitIdle(mpDevice->getDevice());
        vkDestroyImageView(mpDevice->getDevice(), mImageView, nullptr);
    }

    if (viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM) {
        switch (mType) {
        case VK_IMAGE_TYPE_1D:
            viewType = VK_IMAGE_VIEW_TYPE_1D;
            break;
        case VK_IMAGE_TYPE_3D:
            viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        default:
            viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        }
    }

    VkImageViewCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = mImage,
        .viewType = viewType,
        .format = mFormat,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = aspectFlags,
                             .baseMipLevel = 0,
                             .levelCount = mMipLevels,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
    };

    Check::Vk(vkCreateImageView(mpDevice->getDevice(), &ci, nullptr, &mImageView));
}
