#include "Swapchain.h"

#include "Error.h"
#include "Helpers.h"
#include "Log.h"

#include <GLFW/glfw3.h>

static VkPresentModeKHR presentModeNoVsync = VK_PRESENT_MODE_IMMEDIATE_KHR;

using namespace Mandrill;

// Iterate through available formats and return the one that matches what we want. If what we want is not available, the
// first format in the array is returned.
static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (auto& f : availableFormats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return availableFormats[0];
}

// Iterate through available present modes and see if a no v-sync mode is available, otherwise go for
// VK_PRESENT_MODE_FIFO_KHR. If vsync is requested, just go for VK_PRESENT_MODE_FIFO_KHR.
static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool vsync)
{
    if (vsync) {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    for (auto& m : availablePresentModes) {
        if (m == presentModeNoVsync) {
            return m;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

// Set the resolution of the swapchain images. Use the size of the framebuffer from the window.
static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* pWindow)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width;
        int height;
        glfwGetFramebufferSize(pWindow, &width, &height);

        VkExtent2D actualExtent = {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
        };

        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

Swapchain::Swapchain(ptr<Device> pDevice, uint32_t framesInFlight) : mpDevice(pDevice)
{
    querySupport();
    createSwapchain();
    createSyncObjects(framesInFlight);
    createDescriptor();
}

Swapchain::~Swapchain()
{
    destroyDescriptor();
    destroySyncObjects();
    destroySwapchain();
}

void Swapchain::recreate()
{
    Log::debug("Recreating swapchain");

    // Handle minimization
    int width = 0;
    int height = 0;
    do {
        glfwGetFramebufferSize(mpDevice->getWindow(), &width, &height);
        glfwWaitEvents();
    } while (width == 0 || height == 0);

    uint32_t framesInFlight = count(mInFlightFences);
    destroyDescriptor();
    destroySyncObjects();
    destroySwapchain();
    querySupport();
    createSwapchain();
    createSyncObjects(framesInFlight);
    createDescriptor();

    mRecreated = true;
}

VkCommandBuffer Swapchain::acquireNextImage()
{
    // Wait for the current frame to not be in flight
    waitForInFlightImage();
    Check::Vk(vkResetFences(mpDevice->getDevice(), 1, &mInFlightFences[mInFlightIndex]));

    // Acquire index of next image in the swapchain
    VkResult result = vkAcquireNextImageKHR(mpDevice->getDevice(), mSwapchain, UINT64_MAX,
                                            mInFlightSemaphores[mInFlightIndex], nullptr, &mImageIndex);

    // Check if swapchain needs to be reconstructed
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate();
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        Log::error("Failed to acquire next swapchain image");
    }

    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    Check::Vk(vkBeginCommandBuffer(mCommandBuffers[mInFlightIndex], &bi));

    return mCommandBuffers[mInFlightIndex];
}

void Swapchain::present(VkCommandBuffer cmd, ptr<Image> pImage)
{
    if (mRecreated) {
        mRecreated = false;
    }

    // Transition swapchain image for blitting
    Helpers::imageBarrier(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
                          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mImages[mImageIndex]);

    // Blit image to current swapchain image
    VkImageSubresourceLayers subresourceLayers = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    int32_t srcWidth = static_cast<int32_t>(pImage->getWidth());
    int32_t srcHeight = static_cast<int32_t>(pImage->getHeight());
    int32_t dstWidth = static_cast<int32_t>(mExtent.width);
    int32_t dstHeight = static_cast<int32_t>(mExtent.height);
    VkImageBlit2 region = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .srcSubresource = subresourceLayers,
        .srcOffsets = {{0, 0, 0}, {srcWidth, srcHeight, 1}},
        .dstSubresource = subresourceLayers,
        .dstOffsets = {{0, 0, 0}, {dstWidth, dstHeight, 1}},
    };

    VkBlitImageInfo2 blitImageInfo = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .srcImage = pImage->getImage(),
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = mImages[mImageIndex],
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &region,
        .filter = VK_FILTER_NEAREST,
    };

    vkCmdBlitImage2(cmd, &blitImageInfo);

    // Transition swapchain image for presenting
    Helpers::imageBarrier(cmd, VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, mImages[mImageIndex]);

    Check::Vk(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &mInFlightSemaphores[mInFlightIndex],
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &mImageFinishedSemaphores[mImageIndex],
    };

    Check::Vk(vkQueueSubmit(mpDevice->getQueue(), 1, &si, mInFlightFences[mInFlightIndex]));

    VkPresentInfoKHR pi = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &mImageFinishedSemaphores[mImageIndex],
        .swapchainCount = 1,
        .pSwapchains = &mSwapchain,
        .pImageIndices = &mImageIndex,
    };

    VkResult result = vkQueuePresentKHR(mpDevice->getQueue(), &pi);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate();
    } else if (result != VK_SUCCESS) {
        Log::error("Failed to present swapchain image");
    }

    mPreviousInFlightIndex = mInFlightIndex;
    mInFlightIndex = (mInFlightIndex + 1) % count(mInFlightFences);
}

// Query for swapchain support. This function will allocate memory for the pointers in the returned struct and those
// needs to be freed by caller.
void Swapchain::querySupport()
{
    Check::Vk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mpDevice->getPhysicalDevice(), mpDevice->getSurface(),
                                                        &mSupportDetails.capabilities));

    uint32_t count;
    Check::Vk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(mpDevice->getPhysicalDevice(), mpDevice->getSurface(), &count, nullptr));
    mSupportDetails.formats = std::vector<VkSurfaceFormatKHR>(count);
    Check::Vk(vkGetPhysicalDeviceSurfaceFormatsKHR(mpDevice->getPhysicalDevice(), mpDevice->getSurface(), &count,
                                                   mSupportDetails.formats.data()));

    Check::Vk(vkGetPhysicalDeviceSurfacePresentModesKHR(mpDevice->getPhysicalDevice(), mpDevice->getSurface(), &count,
                                                        nullptr));
    mSupportDetails.presentModes = std::vector<VkPresentModeKHR>(count);
    Check::Vk(vkGetPhysicalDeviceSurfacePresentModesKHR(mpDevice->getPhysicalDevice(), mpDevice->getSurface(), &count,
                                                        mSupportDetails.presentModes.data()));
}

void Swapchain::createSwapchain()
{
    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(mSupportDetails.formats);
    VkPresentModeKHR presentMode = choosePresentMode(mSupportDetails.presentModes, mpDevice->getVsync());
    VkExtent2D extent = chooseExtent(mSupportDetails.capabilities, mpDevice->getWindow());

    // Using at least minImageCount number of images is required but using one extra can avoid unnecessary waits on the
    // driver
    uint32_t imageCount = mSupportDetails.capabilities.minImageCount + 1;

    // Also make sure that we are not exceeding the maximum number of images
    if (mSupportDetails.capabilities.maxImageCount > 0 && imageCount > mSupportDetails.capabilities.maxImageCount) {
        imageCount = mSupportDetails.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR ci = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = mpDevice->getSurface(),
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1, // Unless rendering stereoscopically
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .preTransform = mSupportDetails.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    mImageFormat = surfaceFormat.format;
    mExtent = extent;

    Check::Vk(vkCreateSwapchainKHR(mpDevice->getDevice(), &ci, nullptr, &mSwapchain));

    Check::Vk(vkGetSwapchainImagesKHR(mpDevice->getDevice(), mSwapchain, &imageCount, nullptr));
    mImages = std::vector<VkImage>(imageCount);
    mImageViews = std::vector<VkImageView>(imageCount);
    Check::Vk(vkGetSwapchainImagesKHR(mpDevice->getDevice(), mSwapchain, &imageCount, mImages.data()));

    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = mImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = mImageFormat,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1},
        };

        Check::Vk(vkCreateImageView(mpDevice->getDevice(), &ci, nullptr, &mImageViews[i]));
    }
}

void Swapchain::destroySwapchain()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    // Swapchain images are destroyed in vkDestroySwapchainKHR()
    for (auto& iv : mImageViews) {
        vkDestroyImageView(mpDevice->getDevice(), iv, nullptr);
    }

    vkDestroySwapchainKHR(mpDevice->getDevice(), mSwapchain, nullptr);
}

void Swapchain::createSyncObjects(uint32_t framesInFlight)
{
    VkCommandBufferAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = mpDevice->getCommandPool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = framesInFlight,
    };

    mCommandBuffers.resize(framesInFlight);

    Check::Vk(vkAllocateCommandBuffers(mpDevice->getDevice(), &ai, mCommandBuffers.data()));

    mImageFinishedSemaphores.resize(count(mImages));
    mInFlightSemaphores.resize(framesInFlight);
    mInFlightFences.resize(framesInFlight);

    VkSemaphoreCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (uint32_t i = 0; i < count(mImages); i++) {
        Check::Vk(vkCreateSemaphore(mpDevice->getDevice(), &sci, nullptr, &mImageFinishedSemaphores[i]));
    }

    for (uint32_t i = 0; i < framesInFlight; i++) {
        Check::Vk(vkCreateSemaphore(mpDevice->getDevice(), &sci, nullptr, &mInFlightSemaphores[i]));
        Check::Vk(vkCreateFence(mpDevice->getDevice(), &fci, nullptr, &mInFlightFences[i]));
    }
}

void Swapchain::destroySyncObjects()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    vkFreeCommandBuffers(mpDevice->getDevice(), mpDevice->getCommandPool(), count(mCommandBuffers),
                         mCommandBuffers.data());

    for (uint32_t i = 0; i < count(mImages); i++) {
        vkDestroySemaphore(mpDevice->getDevice(), mImageFinishedSemaphores[i], nullptr);
    }

    for (uint32_t i = 0; i < count(mInFlightFences); i++) {
        vkDestroySemaphore(mpDevice->getDevice(), mInFlightSemaphores[i], nullptr);
        vkDestroyFence(mpDevice->getDevice(), mInFlightFences[i], nullptr);
    }
}

void Swapchain::createDescriptor()
{
    const uint32_t copies = count(mImages);

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_ALL,
    };

    VkDescriptorSetLayoutCreateInfo lci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding,
    };

    Check::Vk(vkCreateDescriptorSetLayout(mpDevice->getDevice(), &lci, nullptr, &mDescriptorSetLayout));

    // Create descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, copies},
    };

    VkDescriptorPoolCreateInfo pci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = copies,
        .poolSizeCount = count(poolSizes),
        .pPoolSizes = poolSizes.data(),
    };

    Check::Vk(vkCreateDescriptorPool(mpDevice->getDevice(), &pci, nullptr, &mDescriptorPool));

    std::vector<VkDescriptorSetLayout> layouts(copies, mDescriptorSetLayout);

    // Allocate descriptor sets (one for each swapchain image)
    VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mDescriptorPool,
        .descriptorSetCount = count(layouts),
        .pSetLayouts = layouts.data(),
    };

    mDescriptorSets.resize(copies);
    Check::Vk(vkAllocateDescriptorSets(mpDevice->getDevice(), &ai, mDescriptorSets.data()));

    // Update descriptor sets
    for (uint32_t i = 0; i < count(mImageViews); i++) {
        VkDescriptorImageInfo ii = {
            .imageView = mImageViews[i],
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = mDescriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &ii,
        };

        vkUpdateDescriptorSets(mpDevice->getDevice(), 1, &write, 0, nullptr);
    }
}

void Swapchain::destroyDescriptor()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    vkFreeDescriptorSets(mpDevice->getDevice(), mDescriptorPool, count(mDescriptorSets), mDescriptorSets.data());
    vkDestroyDescriptorPool(mpDevice->getDevice(), mDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(mpDevice->getDevice(), mDescriptorSetLayout, nullptr);
}
