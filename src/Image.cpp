#include "Image.h"

#include "Error.h"

using namespace Mandrill;

Image::Image(VkDevice device, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits samples,
             VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
    : mDevice(device), mImageView(VK_NULL_HANDLE)
{
    VkImageCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = samples,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    Check::Vk(vkCreateImage(device, &ci, nullptr, &mImage));
}

Image::~Image()
{
    if (mImageView) {
        vkDestroyImageView(mDevice, mImageView, nullptr);
    }
    vkDestroyImage(mDevice, mImage, nullptr);
}

void Image::createImageView(VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
    VkImageViewCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = mImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = aspectFlags,
                             .baseMipLevel = 0,
                             .levelCount = mipLevels,
                             .baseArrayLayer = 0},
    };

    Check::Vk(vkCreateImageView(mDevice, &ci, nullptr, &mImageView));
}