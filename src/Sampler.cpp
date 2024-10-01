#include "Sampler.h"

#include "Error.h"

using namespace Mandrill;

Sampler::Sampler(ptr<Device> pDevice, VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode,
                 VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                 VkSamplerAddressMode addressModeW)
    : mpDevice(pDevice)
{
    VkSamplerCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = magFilter,
        .minFilter = minFilter,
        .mipmapMode = mipmapMode,
        .addressModeU = addressModeU,
        .addressModeV = addressModeV,
        .addressModeW = addressModeW,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = mpDevice->getProperties().physicalDevice.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 1000.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    Check::Vk(vkCreateSampler(mpDevice->getDevice(), &ci, nullptr, &mSampler));
}

Sampler::~Sampler()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    vkDestroySampler(mpDevice->getDevice(), mSampler, nullptr);
}
