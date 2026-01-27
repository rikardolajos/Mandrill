#pragma once

#include "Common.h"

#include "Device.h"
#include "Log.h"

namespace Mandrill
{
    /// <summary>
    /// Buffer class for managing Vulkan buffers. This class handles the creation and destruction of buffers, as well as
    /// basic memory management.
    /// </summary>
    class Buffer
    {
    public:
        MANDRILL_NON_COPYABLE(Buffer)

        /// <summary>
        /// Create a new buffer.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="size">Size of buffer in bytes</param>
        /// <param name="usage">How the buffer will be used</param>
        /// <param name="properties">What properties the memory should have</param>
        MANDRILL_API Buffer(ptr<Device> pDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties);

        /// <summary>
        /// Destructor for buffer.
        /// </summary>
        MANDRILL_API ~Buffer();

        /// <summary>
        /// Copy data from host to the buffer. If the buffer was not created to have host-coherent memory, a staging
        /// buffer will be used to transfer the data.
        /// </summary>
        /// <param name="pData">Data to copy</param>
        /// <param name="size">Size of data to copy in bytes</param>
        /// <param name="offset">Offset into the buffer where to place the data</param>
        MANDRILL_API void copyFromHost(const void* pData, VkDeviceSize size, VkDeviceSize offset = 0);

        /// <summary>
        /// Get the buffer handle.
        /// </summary>
        /// <returns>Vulkan buffer handle</returns>
        MANDRILL_API VkBuffer getBuffer() const
        {
            return mBuffer;
        }

        /// <summary>
        /// Get the memory handle.
        /// </summary>
        /// <returns>Vulkan device memory handle</returns>
        MANDRILL_API VkDeviceMemory getMemory() const
        {
            return mMemory;
        }

        /// <summary>
        /// Get the buffer usage flags.
        /// </summary>
        /// <returns>Usage flags</returns>
        MANDRILL_API VkBufferUsageFlags getUsage() const
        {
            return mUsage;
        }

        /// <summary>
        /// Get the memory property flags
        /// </summary>
        /// <returns>Memory property flags</returns>
        MANDRILL_API VkMemoryPropertyFlags getProperties() const
        {
            return mProperties;
        }

        /// <summary>
        /// Get the device address of the buffer.
        /// </summary>
        /// <returns>A vulkan device address</returns>
        MANDRILL_API VkDeviceAddress getDeviceAddress() const
        {
            VkBufferDeviceAddressInfo ai = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = mBuffer,
            };
            return vkGetBufferDeviceAddress(mpDevice->getDevice(), &ai);
        }

        /// <summary>
        /// If the buffer memory is host-coherent, this returns the pointor to the memory.
        /// </summary>
        /// <returns>Pointer to buffer memory, or nullptr</returns>
        MANDRILL_API void* getHostMap() const
        {
            if (!(mProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                Log::Error("Unable to access host map of buffer that is not host coherent.");
                return nullptr;
            }
            return mpHostMap;
        }

        /// <summary>
        /// Get the size of the buffer in bytes.
        /// </summary>
        /// <returns>Size of the buffer</returns>
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
