#pragma once

#include "Common.h"

#include "AccelerationStructure.h"
#include "Buffer.h"
#include "Device.h"
#include "Log.h"
#include "Texture.h"

namespace Mandrill
{
    struct DescriptorDesc {
        VkDescriptorType type;
        std::variant<ptr<Buffer>, ptr<Image>, ptr<Texture>, ptr<std::vector<ptr<Texture>>>, ptr<AccelerationStructure>>
            pResource;
        VkDeviceSize offset = 0;
        VkDeviceSize range;
        VkBufferView bufferView = nullptr;
        VkImageView imageView = nullptr;
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t arrayCount;

        MANDRILL_API DescriptorDesc(VkDescriptorType type, ptr<void> pResource, VkDeviceSize offset = 0,
                                    VkDeviceSize range = VK_WHOLE_SIZE, uint32_t arrayCount = 0)
            : type(type), offset(offset), range(range), arrayCount(arrayCount)
        {
            switch (type) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                this->pResource = std::static_pointer_cast<Buffer>(pResource);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                this->pResource = std::static_pointer_cast<Image>(pResource);
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                if (arrayCount) {
                    this->pResource = std::static_pointer_cast<std::vector<ptr<Texture>>>(pResource);
                } else {
                    this->pResource = std::static_pointer_cast<Texture>(pResource);
                }
                break;
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                this->pResource = std::static_pointer_cast<AccelerationStructure>(pResource);
                break;
            default:
                Log::Error("DescriptorDesc: Resource not supported\n");
            }
        }

        MANDRILL_API ~DescriptorDesc()
        {
        }
    };

    class Descriptor
    {
    public:
        MANDRILL_NON_COPYABLE(Descriptor)

        /// <summary>
        /// Create a new descriptor.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="desc">Description of the descriptor being created</param>
        /// <param name="layout">Layout to use</param>
        MANDRILL_API Descriptor(ptr<Device> pDevice, const std::vector<DescriptorDesc>& desc,
                                VkDescriptorSetLayout layout);

        /// <summary>
        /// Destructor for descriptor.
        /// </summary>
        MANDRILL_API ~Descriptor();

        /// <summary>
        /// Bind descriptor with vector of dynamic offsets.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="bindPoint">Bind point in pipeline</param>
        /// <param name="pipelineLayout">Layout of pipeline</param>
        /// <param name="firstSet">At which index to put the start of the descriptor</param>
        /// <param name="offsets">Vector of dynamic offsets</param>
        /// <returns></returns>
        MANDRILL_API void bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout,
                               uint32_t firstSet, std::vector<uint32_t> offsets)
        {
            vkCmdBindDescriptorSets(cmd, bindPoint, pipelineLayout, firstSet, 1, &mSet, count(offsets), offsets.data());
        }

        /// <summary>
        /// Bind descriptor with one dynamic offset.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="bindPoint">Bind point in pipeline</param>
        /// <param name="pipelineLayout">Layout of pipeline</param>
        /// <param name="firstSet">At which index to put the start of the descriptor</param>
        /// <param name="offset">A dynamic offset</param>
        /// <returns></returns>
        MANDRILL_API void bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout,
                               uint32_t firstSet, uint32_t offset)
        {
            std::vector<uint32_t> offsets = {offset};
            bind(cmd, bindPoint, pipelineLayout, firstSet, offsets);
        }

        /// <summary>
        /// Bind descriptor.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="bindPoint">Bind point in pipeline</param>
        /// <param name="pipelineLayout">Layout of pipeline</param>
        /// <param name="firstSet">At which index to put the start of the descriptor</param>
        /// <returns></returns>
        MANDRILL_API void bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VkPipelineLayout pipelineLayout,
                               uint32_t firstSet)
        {
            vkCmdBindDescriptorSets(cmd, bindPoint, pipelineLayout, firstSet, 1, &mSet, 0, nullptr);
        }

        /// <summary>
        /// Get the descriptor set handle.
        /// </summary>
        /// <returns>Descriptor set handle</returns>
        MANDRILL_API VkDescriptorSet getSet() const
        {
            return mSet;
        }

    private:
        void allocate(const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout);

        ptr<Device> mpDevice;

        VkDescriptorPool mPool;
        VkDescriptorSet mSet;
    };
} // namespace Mandrill
