#pragma once

#include "Common.h"

#include "Device.h"

namespace Mandrill
{
    class Buffer
    {
    public:
        MANDRILL_API Buffer(std::shared_ptr<Device> pDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties);
        MANDRILL_API ~Buffer();

        MANDRILL_API void copyFromHost(const void* data, size_t size);

        MANDRILL_API VkBuffer getBuffer() const
        {
            return mBuffer;
        }

        MANDRILL_API VkDeviceMemory getMemory() const
        {
            return mMemory;
        }

        MANDRILL_API VkDeviceAddress getDeviceAddress() const
        {
            VkBufferDeviceAddressInfo ai = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = mBuffer,
            };
            return vkGetBufferDeviceAddress(mpDevice->getDevice(), &ai);
        }

    private:
        std::shared_ptr<Device> mpDevice;

        VkBuffer mBuffer;
        VkDeviceMemory mMemory;
        VkBufferUsageFlags mUsage;
        VkMemoryPropertyFlags mProperties;
        VkDeviceSize mSize;
    };
} // namespace Mandrill