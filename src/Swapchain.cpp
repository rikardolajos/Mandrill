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

    uint32_t framesInFlight = static_cast<uint32_t>(mInFlightFences.size());
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
    Check::Vk(vkWaitForFences(mpDevice->getDevice(), 1, &mInFlightFences[mInFlightIndex], VK_TRUE, UINT64_MAX));

    // Acquire index of next image in the swapchain
    VkResult result = vkAcquireNextImageKHR(mpDevice->getDevice(), mSwapchain, UINT64_MAX,
                                            mImageAvailableSemaphore[mInFlightIndex], nullptr, &mImageIndex);

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

void Swapchain::present(VkCommandBuffer cmd)
{
    if (mRecreated) {
        mRecreated = false;
    }

    Check::Vk(vkEndCommandBuffer(cmd));

    std::array<VkSemaphore, 1> waitSemaphores{mImageAvailableSemaphore[mInFlightIndex]};
    std::array<VkSemaphore, 1> signalSemaphores{mRenderFinishedSemaphore[mInFlightIndex]};
    std::array<VkPipelineStageFlags, 1> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
        .pWaitSemaphores = waitSemaphores.data(),
        .pWaitDstStageMask = waitStages.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
        .pSignalSemaphores = signalSemaphores.data(),
    };

    Check::Vk(vkResetFences(mpDevice->getDevice(), 1, &mInFlightFences[mInFlightIndex]));

    Check::Vk(vkQueueSubmit(mpDevice->getQueue(), 1, &si, mInFlightFences[mInFlightIndex]));

    std::array<VkSwapchainKHR, 1> swapchains{mSwapchain};

    VkPresentInfoKHR pi = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size()),
        .pWaitSemaphores = signalSemaphores.data(),
        .swapchainCount = static_cast<uint32_t>(swapchains.size()),
        .pSwapchains = swapchains.data(),
        .pImageIndices = &mImageIndex,
    };

    VkResult result = vkQueuePresentKHR(mpDevice->getQueue(), &pi);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate();
    } else if (result != VK_SUCCESS) {
        Log::error("Failed to present swapchain image");
    }

    mPreviousInFlightIndex = mInFlightIndex;
    mInFlightIndex = (mInFlightIndex + 1) % static_cast<uint32_t>(mInFlightFences.size());
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
        .imageUsage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
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

    mImageAvailableSemaphore.resize(framesInFlight);
    mRenderFinishedSemaphore.resize(framesInFlight);
    mInFlightFences.resize(framesInFlight);

    VkSemaphoreCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fci = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (uint32_t i = 0; i < framesInFlight; i++) {
        Check::Vk(vkCreateSemaphore(mpDevice->getDevice(), &sci, nullptr, &mImageAvailableSemaphore[i]));
        Check::Vk(vkCreateSemaphore(mpDevice->getDevice(), &sci, nullptr, &mRenderFinishedSemaphore[i]));
        Check::Vk(vkCreateFence(mpDevice->getDevice(), &fci, nullptr, &mInFlightFences[i]));
    }
}

void Swapchain::destroySyncObjects()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    vkFreeCommandBuffers(mpDevice->getDevice(), mpDevice->getCommandPool(),
                         static_cast<uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());

    for (uint32_t i = 0; i < mImageAvailableSemaphore.size(); i++) {
        vkDestroySemaphore(mpDevice->getDevice(), mImageAvailableSemaphore[i], nullptr);
        vkDestroySemaphore(mpDevice->getDevice(), mRenderFinishedSemaphore[i], nullptr);
        vkDestroyFence(mpDevice->getDevice(), mInFlightFences[i], nullptr);
    }
}

void Swapchain::createDescriptor()
{
    const uint32_t copies = static_cast<uint32_t>(mImages.size());

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
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    Check::Vk(vkCreateDescriptorPool(mpDevice->getDevice(), &pci, nullptr, &mDescriptorPool));

    std::vector<VkDescriptorSetLayout> layouts(copies, mDescriptorSetLayout);

    // Allocate descriptor sets (one for each swapchain image)
    VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };

    mDescriptorSets.resize(copies);
    Check::Vk(vkAllocateDescriptorSets(mpDevice->getDevice(), &ai, mDescriptorSets.data()));

    // Update descriptor sets
    for (uint32_t i = 0; i < static_cast<uint32_t>(mImageViews.size()); i++) {
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

    vkFreeDescriptorSets(mpDevice->getDevice(), mDescriptorPool, static_cast<uint32_t>(mDescriptorSets.size()),
                         mDescriptorSets.data());
    vkDestroyDescriptorPool(mpDevice->getDevice(), mDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(mpDevice->getDevice(), mDescriptorSetLayout, nullptr);
}
