#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "Error.h"
#include "Image.h"
#include "Log.h"

namespace Mandrill
{
    class MANDRILL_API Helpers
    {
    public:
        inline static VkCommandBuffer cmdBegin(std::shared_ptr<Device> pDevice)
        {
            VkCommandBufferAllocateInfo ai = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = pDevice->getCommandPool(),
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };

            VkCommandBuffer cmd;
            Check::Vk(vkAllocateCommandBuffers(pDevice->getDevice(), &ai, &cmd));

            VkCommandBufferBeginInfo bi = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };

            Check::Vk(vkBeginCommandBuffer(cmd, &bi));

            return cmd;
        }

        inline static void cmdEnd(std::shared_ptr<Device> pDevice, VkCommandBuffer cmd)
        {
            Check::Vk(vkEndCommandBuffer(cmd));

            VkSubmitInfo si = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .commandBufferCount = 1,
                .pCommandBuffers = &cmd,
            };

            Check::Vk(vkQueueSubmit(pDevice->getQueue(), 1, &si, nullptr));
            Check::Vk(vkQueueWaitIdle(pDevice->getQueue()));

            vkFreeCommandBuffers(pDevice->getDevice(), pDevice->getCommandPool(), 1, &cmd);
        }

        inline static uint32_t findMemoryType(std::shared_ptr<Device> pDevice, uint32_t typeFilter,
                                              VkMemoryPropertyFlags properties)
        {
            for (uint32_t i = 0; i < pDevice->getProperties().memory.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) &&
                    (pDevice->getProperties().memory.memoryTypes[i].propertyFlags & properties) == properties) {
                    return i;
                }
            }

            Log::error("Failed to find suitable memory type");
            return UINT32_MAX;
        }

        inline static VkFormat findSupportedFormat(std::shared_ptr<Device> pDevice, std::vector<VkFormat> candidates,
                                                   VkImageTiling tiling, VkFormatFeatureFlags features)
        {
            for (auto& c : candidates) {
                VkFormatProperties properties;
                vkGetPhysicalDeviceFormatProperties(pDevice->getPhysicalDevice(), c, &properties);

                if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) {
                    return c;
                } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                           (properties.optimalTilingFeatures & features) == features) {
                    return c;
                }
            }
            return VK_FORMAT_UNDEFINED;
        }


        inline static VkFormat findDepthFormat(std::shared_ptr<Device> pDevice)
        {
            std::vector<VkFormat> candidates;
            candidates.push_back(VK_FORMAT_D32_SFLOAT);
            candidates.push_back(VK_FORMAT_D32_SFLOAT_S8_UINT);
            candidates.push_back(VK_FORMAT_D24_UNORM_S8_UINT);

            return findSupportedFormat(pDevice, candidates, VK_IMAGE_TILING_OPTIMAL,
                                       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }

        inline static void transitionImageLayout(std::shared_ptr<Device> pDevice, VkImage image, VkFormat format,
                                                 VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
        {
            VkCommandBuffer cmd = cmdBegin(pDevice);

            VkImageMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask = 0, // Set below
                .dstAccessMask = 0, // Set below
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange =
                    {
                        .aspectMask = 0, // Set below
                        .baseMipLevel = 0,
                        .levelCount = mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
            };

            VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_NONE_KHR;
            VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_NONE_KHR;

            if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

                // Check if format has stencil component
                if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
                    barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
            } else {
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            }

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                       newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                       newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask =
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            } else {
                Log::error("Unsupported layout transition");
            }

            vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            cmdEnd(pDevice, cmd);
        }

        inline static void copyBufferToImage(std::shared_ptr<Device> pDevice, const Buffer& buffer, const Image& image,
                                             uint32_t width, uint32_t height)
        {
            VkCommandBuffer cmd = cmdBegin(pDevice);

            VkBufferImageCopy region = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .imageOffset = {0, 0},
                .imageExtent = {width, height, 1},
            };

            vkCmdCopyBufferToImage(cmd, buffer.getBuffer(), image.getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &region);

            cmdEnd(pDevice, cmd);
        }
    };
} // namespace Mandrill