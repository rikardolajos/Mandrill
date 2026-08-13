#include "DynamicBuffer.h"

#include "Helpers.h"
#include "Log.h"

using namespace Mandrill;

namespace
{
    // Which limit the copies have to be aligned to depends on what the buffer is bound as. A buffer that is used as
    // both takes the stricter of the two.
    VkDeviceSize getRequiredAlignment(ptr<Device> pDevice, VkBufferUsageFlags usage)
    {
        const VkPhysicalDeviceLimits& limits = pDevice->getProperties().physicalDevice.limits;

        VkDeviceSize alignment = 1;
        if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
            alignment = std::max(alignment, limits.minUniformBufferOffsetAlignment);
        }
        if (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
            alignment = std::max(alignment, limits.minStorageBufferOffsetAlignment);
        }
        return alignment;
    }
} // namespace

DynamicBuffer::DynamicBuffer(ptr<Device> pDevice, VkDeviceSize elementSize, uint32_t elementCount,
                             VkBufferUsageFlags usage)
    : mpDevice(pDevice), mElementSize(elementSize), mElementCount(elementCount)
{
    if (mElementSize == 0) {
        Log::Error("DynamicBuffer: Element size has to be at least 1 byte");
        mElementSize = 1;
    }

    // An empty buffer cannot be allocated, and a scene that has not been filled in yet is a normal state to be in
    if (mElementCount == 0) {
        mElementCount = 1;
    }

    mStride = Helpers::alignTo(mElementSize, getRequiredAlignment(pDevice, usage));

    mpBuffer = pDevice->createBuffer(mStride * mElementCount, usage,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

DynamicBuffer::~DynamicBuffer()
{
}

void* DynamicBuffer::at(uint32_t index) const
{
    if (index >= mElementCount) {
        Log::Error("DynamicBuffer: Element {} is out of range, the buffer holds {} elements", index, mElementCount);
        return nullptr;
    }

    return static_cast<std::byte*>(mpBuffer->getHostMap()) + mStride * index;
}

void DynamicBuffer::copyFromHost(const void* pData, uint32_t index)
{
    void* pElement = at(index);
    if (!pElement) {
        return; // Out of range, already reported
    }

    std::memcpy(pElement, pData, mElementSize);
}

uint32_t DynamicBuffer::getOffset(uint32_t index) const
{
    if (index >= mElementCount) {
        Log::Error("DynamicBuffer: Element {} is out of range, the buffer holds {} elements", index, mElementCount);
        return 0;
    }

    return static_cast<uint32_t>(mStride * index);
}
