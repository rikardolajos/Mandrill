#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"

namespace Mandrill
{
    /// <summary>
    /// Buffer holding several equally sized copies of the same data, where each copy starts on a boundary that the
    /// device accepts as a descriptor offset. Use it for data that the host rewrites while the device may still be
    /// reading the previous value, which typically means one copy per frame in flight.
    ///
    /// The copies are addressed by index, on the host with at() and on the device with getOffset(), so the alignment
    /// never has to be repeated at the call site and the host and the device can never disagree on the stride.
    ///
    /// A resource that is indexed by more than one thing, such as a node transform that varies both per node and per
    /// frame in flight, uses one element per combination and lays them out with the fastest varying index last.
    /// </summary>
    class DynamicBuffer
    {
    public:
        MANDRILL_NON_COPYABLE(DynamicBuffer)

        /// <summary>
        /// Create a new dynamic buffer.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="elementSize">Size of one copy in bytes</param>
        /// <param name="elementCount">Number of copies to allocate</param>
        /// <param name="usage">How the buffer will be used, which also decides the alignment of the copies</param>
        MANDRILL_API DynamicBuffer(ptr<Device> pDevice, VkDeviceSize elementSize, uint32_t elementCount,
                                   VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        /// <summary>
        /// Destructor for dynamic buffer.
        /// </summary>
        MANDRILL_API ~DynamicBuffer();

        /// <summary>
        /// Get a host pointer to one of the copies.
        /// </summary>
        /// <param name="index">Which copy to address. Defaults to the copy of the current frame in flight, which only
        /// works for a buffer that holds exactly one copy per frame.</param>
        /// <returns>Pointer to the copy, or nullptr if the index is out of range</returns>
        MANDRILL_API void* at(uint32_t index = kCurrentFrameInFlight) const;

        /// <summary>
        /// Write one of the copies. Copies getElementSize() bytes from the given data.
        /// </summary>
        /// <param name="pData">Data to copy</param>
        /// <param name="index">Which copy to write. Defaults to the copy of the current frame in flight, which only
        /// works for a buffer that holds exactly one copy per frame.</param>
        MANDRILL_API void copyFromHost(const void* pData, uint32_t index = kCurrentFrameInFlight);

        /// <summary>
        /// Get the dynamic offset that selects one of the copies when binding a descriptor.
        /// </summary>
        /// <param name="index">Which copy to address. Defaults to the copy of the current frame in flight, which only
        /// works for a buffer that holds exactly one copy per frame.</param>
        /// <returns>Offset in bytes from the start of the buffer</returns>
        MANDRILL_API uint32_t getOffset(uint32_t index = kCurrentFrameInFlight) const;

        /// <summary>
        /// Check whether the buffer holds exactly one copy per frame in flight, which is what makes it possible to
        /// address it by the current frame alone.
        /// </summary>
        /// <returns>True if there is one copy per frame in flight</returns>
        MANDRILL_API bool isPerFrame() const;

        /// <summary>
        /// Get the distance in bytes between two consecutive copies. This is the element size rounded up to the
        /// alignment the device requires of descriptor offsets, so it is not necessarily the element size.
        /// </summary>
        /// <returns>Stride in bytes</returns>
        MANDRILL_API VkDeviceSize getStride() const
        {
            return mStride;
        }

        /// <summary>
        /// Get the size of one copy. This is the range a descriptor should expose.
        /// </summary>
        /// <returns>Size of one copy in bytes</returns>
        MANDRILL_API VkDeviceSize getElementSize() const
        {
            return mElementSize;
        }

        /// <summary>
        /// Get the number of copies in the buffer.
        /// </summary>
        /// <returns>Number of copies</returns>
        MANDRILL_API uint32_t getElementCount() const
        {
            return mElementCount;
        }

        /// <summary>
        /// Get the underlying buffer.
        /// </summary>
        /// <returns>Buffer holding all the copies</returns>
        MANDRILL_API ptr<Buffer> getBuffer() const
        {
            return mpBuffer;
        }

    private:
        // Turns kCurrentFrameInFlight into the frame the device is on, and reports if that cannot mean anything for
        // this buffer because it is not laid out one copy per frame
        uint32_t resolveIndex(uint32_t index) const;

        ptr<Device> mpDevice;
        ptr<Buffer> mpBuffer;

        VkDeviceSize mElementSize;
        VkDeviceSize mStride;
        uint32_t mElementCount;
    };
} // namespace Mandrill
