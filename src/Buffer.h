#pragma once

#include "Common.h"

#include "Device.h"
#include "Log.h"

namespace Mandrill
{
    class Buffer
    {
    public:
        MANDRILL_API Buffer(ptr<Device> pDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties);
        MANDRILL_API ~Buffer();

        MANDRILL_API void copyFromHost(const void* pData, VkDeviceSize size, VkDeviceSize offset = 0);

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

        MANDRILL_API void* getHostMap() const
        {
            if (!(mProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                Log::error("Unable to access host map of buffer that is not host coherent.");
            }
            return mpHostMap;
        }

        MANDRILL_API VkDeviceSize getSize() const
        {
            return mSize;
        }

    private:
        ptr<Device> mpDevice;

        VkBuffer mBuffer;
        VkDeviceMemory mMemory;
        VkBufferUsageFlags mUsage;
        VkMemoryPropertyFlags mProperties;
        VkDeviceSize mSize;
        void* mpHostMap;
    };
} // namespace Mandrill