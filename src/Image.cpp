#include "Image.h"

#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

Image::Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
             VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
             VkMemoryPropertyFlags properties)
    : mpDevice(pDevice), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(mipLevels), mFormat(format),
      mTiling(tiling), mUsage(usage), mProperties(properties), mImageView(VK_NULL_HANDLE), mOwnMemory(true), mpHostMap(nullptr)
{
    VkImageCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = mHeight == 1  ? VK_IMAGE_TYPE_1D
                     : mDepth == 1 ? VK_IMAGE_TYPE_2D
                                   : VK_IMAGE_TYPE_3D,
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
}

Image::Image(ptr<Device> pDevice, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
             VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
             VkDeviceMemory memory, VkDeviceSize offset)
    : mpDevice(pDevice), mWidth(width), mHeight(height), mDepth(depth), mMipLevels(mipLevels), mFormat(format),
      mTiling(tiling), mUsage(usage), mProperties(0), mImageView(VK_NULL_HANDLE), mMemory(memory), mOwnMemory(false),
      mpHostMap(nullptr)
{
    VkImageCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = mHeight == 1  ? VK_IMAGE_TYPE_1D
                     : mDepth == 1 ? VK_IMAGE_TYPE_2D
                                   : VK_IMAGE_TYPE_3D,
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

void Image::createImageView(VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = mImage,
        .viewType = mHeight == 1  ? VK_IMAGE_VIEW_TYPE_1D
                    : mDepth == 1 ? VK_IMAGE_VIEW_TYPE_2D
                                  : VK_IMAGE_VIEW_TYPE_3D,
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
