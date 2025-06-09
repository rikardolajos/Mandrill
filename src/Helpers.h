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
        /// <summary>
        /// Create a new one-time use command buffer. This call should be paired with a corresponding call to cmdEnd().
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <returns>Command buffer</returns>
        inline static VkCommandBuffer cmdBegin(ptr<Device> pDevice)
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

        /// <summary>
        /// End a one-time use command buffer that was initialized with a call to cmdBegin().
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="cmd">Command buffer to end</param>
        inline static void cmdEnd(ptr<Device> pDevice, VkCommandBuffer cmd)
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

        /// <summary>
        /// Find a suitable memory type given the memory properties.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="typeFilter">Type filter from memory requirements</param>
        /// <param name="properties">Requested memory properties</param>
        /// <returns>Memory type</returns>
        inline static uint32_t findMemoryType(ptr<Device> pDevice, uint32_t typeFilter,
                                              VkMemoryPropertyFlags properties)
        {
            for (uint32_t i = 0; i < pDevice->getProperties().memory.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) &&
                    (pDevice->getProperties().memory.memoryTypes[i].propertyFlags & properties) == properties) {
                    return i;
                }
            }

            Log::Error("Failed to find suitable memory type");
            return UINT32_MAX;
        }

        /// <summary>
        /// Find supported format for an attachment.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="candidates">Formats that we want to use</param>
        /// <param name="tiling">Tiling to use</param>
        /// <param name="features">Features to use</param>
        /// <returns>A supported format or VK_FORMAT_UNDEFINED</returns>
        inline static VkFormat findSupportedFormat(ptr<Device> pDevice, std::vector<VkFormat> candidates,
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

        /// <summary>
        /// Find a supported depth format for a depth attachment.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <returns>A supported depth format or VK_FORMAT_UNDEFINED</returns>
        inline static VkFormat findDepthFormat(ptr<Device> pDevice)
        {
            std::vector<VkFormat> candidates;
            candidates.push_back(VK_FORMAT_D32_SFLOAT);
            candidates.push_back(VK_FORMAT_D32_SFLOAT_S8_UINT);
            candidates.push_back(VK_FORMAT_D24_UNORM_S8_UINT);

            return findSupportedFormat(pDevice, candidates, VK_IMAGE_TILING_OPTIMAL,
                                       VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }

        /// <summary>
        /// Create an image barrier with layout transition.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="image">Image to transition</param>
        /// <param name="srcStage">Stage the barrier has to wait for</param>
        /// <param name="srcAccess">Type of access the barrier has to wait for</param>
        /// <param name="dstStage">Stage when the barrier has to be finished</param>
        /// <param name="dstAccess">Type of access the barrer must be ready for when finished</param>
        /// <param name="oldLayout">Old image layout</param>
        /// <param name="newLayout">New image layout</param>
        /// <param name="pSubresourceRange">Subresource range to use, or nullptr</param>
        inline static void imageBarrier(VkCommandBuffer cmd, VkImage image, VkPipelineStageFlags2 srcStage,
                                        VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                                        VkAccessFlags2 dstAccess, VkImageLayout oldLayout, VkImageLayout newLayout,
                                        VkImageSubresourceRange* pSubresourceRange = nullptr)
        {
            VkImageSubresourceRange defaultSubresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            };

            VkImageMemoryBarrier2 barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = srcStage,
                .srcAccessMask = srcAccess,
                .dstStageMask = dstStage,
                .dstAccessMask = dstAccess,
                .oldLayout = oldLayout,
                .newLayout = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = pSubresourceRange ? *pSubresourceRange : defaultSubresourceRange,
            };

            VkDependencyInfo dependencyInfo = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &barrier,
            };

            vkCmdPipelineBarrier2(cmd, &dependencyInfo);
        }

        /// <summary>
        /// Copy a buffer to an image.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="buffer">Buffer to use</param>
        /// <param name="image">Image to use</param>
        /// <param name="width">Width of image</param>
        /// <param name="height">Height of image</param>
        /// <param name="depth">Depth of image</param>
        inline static void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width,
                                             uint32_t height, uint32_t depth)
        {
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
                .imageExtent = {width, height, depth},
            };

            vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        /// <summary>
        /// Copy an image to a buffer.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="image">Image to use</param>
        /// <param name="buffer">Buffer to use</param>
        /// <param name="width">Width of image</param>
        /// <param name="height">Height of image</param>
        /// <param name="depth">Depth of image</param>
        inline static void copyImageToBuffer(ptr<Device> pDevice, VkImage image, VkBuffer buffer, uint32_t width,
                                             uint32_t height, uint32_t depth)
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
                .imageExtent = {width, height, depth},
            };

            vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &region);

            cmdEnd(pDevice, cmd);
        }

        /// <summary>
        /// Aligne a value to a given alignment.
        /// </summary>
        /// <param name="value">Value to align</param>
        /// <param name="alignment">Alignment to use</param>
        /// <returns>Aligned value</returns>
        inline static VkDeviceSize alignTo(VkDeviceSize value, VkDeviceSize alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        /// <summary>
        /// Return a random value from the interval [0.0, 1.0)
        /// </summary>
        /// <param name="reset">If true, the random seed is reset</param>
        /// <returns>A random value</returns>
        inline float random(bool reset = false)
        {
            static std::random_device dev;
            static std::mt19937 rng(dev());
            static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            if (reset) {
                rng.seed();
            }
            return dis(rng);
        }
    };
} // namespace Mandrill
