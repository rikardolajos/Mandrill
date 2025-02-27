#include "Image.h"

#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

Image::Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits samples,
             VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
    : mpDevice(pDevice), mWidth(width), mHeight(height), mMipLevels(mipLevels), mFormat(format), mTiling(tiling),
      mImageView(VK_NULL_HANDLE)
{
    VkImageCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = mFormat,
        .extent = {.width = mWidth, .height = mHeight, .depth = 1},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = mTiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    Check::Vk(vkCreateImage(mpDevice->getDevice(), &ci, nullptr, &mImage));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(mpDevice->getDevice(), mImage, &memReqs);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = Helpers::findMemoryType(mpDevice, memReqs.memoryTypeBits, properties),
    };

    Check::Vk(vkAllocateMemory(mpDevice->getDevice(), &allocInfo, nullptr, &mMemory));

    mOwnMemory = true;

    Check::Vk(vkBindImageMemory(mpDevice->getDevice(), mImage, mMemory, 0));
}

Image::Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits samples,
             VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkDeviceMemory memory, VkDeviceSize offset)
    : mpDevice(pDevice), mWidth(width), mHeight(height), mMipLevels(mipLevels), mFormat(format), mTiling(tiling),
      mImageView(VK_NULL_HANDLE), mMemory(memory)
{
    VkImageCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {.width = mWidth, .height = mHeight, .depth = 1},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    Check::Vk(vkCreateImage(mpDevice->getDevice(), &ci, nullptr, &mImage));

    mOwnMemory = false;

    Check::Vk(vkBindImageMemory(mpDevice->getDevice(), mImage, mMemory, offset));
}

Image::~Image()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    if (mOwnMemory) {
        vkFreeMemory(mpDevice->getDevice(), mMemory, nullptr);
    }

    if (mImageView) {
        vkDestroyImageView(mpDevice->getDevice(), mImageView, nullptr);
    }

    vkDestroyImage(mpDevice->getDevice(), mImage, nullptr);
}

void Image::createImageView(VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = mImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
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
