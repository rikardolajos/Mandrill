#include "Buffer.h"

#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

Buffer::Buffer(std::shared_ptr<Device> pDevice, VkDeviceSize size, VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties)
    : mpDevice(pDevice), mSize(size), mUsage(usage), mProperties(properties)
{
    VkBufferCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = mSize,
        .usage = mUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    Check::Vk(vkCreateBuffer(mpDevice->getDevice(), &ci, nullptr, &mBuffer));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(mpDevice->getDevice(), mBuffer, &memReqs);

    VkMemoryAllocateFlagsInfo allocFlagInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
    };

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex =
            Helpers::findMemoryType(mpDevice, memReqs.memoryTypeBits, mProperties),
    };

    Check::Vk(vkAllocateMemory(mpDevice->getDevice(), &allocInfo, nullptr, &mMemory));

    Check::Vk(vkBindBufferMemory(mpDevice->getDevice(), mBuffer, mMemory, 0));
}

Buffer::~Buffer()
{
    vkDeviceWaitIdle(mpDevice->getDevice());

    vkDestroyBuffer(mpDevice->getDevice(), mBuffer, nullptr);
    vkFreeMemory(mpDevice->getDevice(), mMemory, nullptr);
}

void Buffer::copyFromHost(const void* data, size_t size)
{
    // Check if we need a staging buffer or not
    if (!(mProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        // Set up staging buffer
        Buffer staging(mpDevice, mSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        void* dstData;
        Check::Vk(vkMapMemory(mpDevice->getDevice(), staging.getMemory(), 0, size, 0, &dstData));
        std::memcpy(dstData, data, size);
        vkUnmapMemory(mpDevice->getDevice(), staging.getMemory());

        VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);

        VkBufferCopy region = {
            .size = size,
        };
        vkCmdCopyBuffer(cmd, staging.getBuffer(), mBuffer, 1, &region);

        Helpers::cmdEnd(mpDevice, cmd);

    } else {
        // Transfer directly without staging buffer
        void* dstData;
        Check::Vk(vkMapMemory(mpDevice->getDevice(), mMemory, 0, size, 0, &dstData));
        std::memcpy(dstData, data, size);
        vkUnmapMemory(mpDevice->getDevice(), mMemory);
    }
}
