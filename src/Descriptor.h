#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "Log.h"
#include "Texture.h"

namespace Mandrill
{
    struct DescriptorDesc {
        VkDescriptorType type;
        std::variant<ptr<Buffer>, ptr<Image>, ptr<Texture>> pResource;
        VkImageView imageView = nullptr;

        MANDRILL_API DescriptorDesc(VkDescriptorType type, ptr<void> pResource) : type(type)
        {
            switch (type) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                this->pResource = std::static_pointer_cast<Buffer>(pResource);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                this->pResource = std::static_pointer_cast<Image>(pResource);
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                this->pResource = std::static_pointer_cast<Texture>(pResource);
                break;
            default:
                Log::error("DescriptorDesc: Resource not supported\n");
            }
        }

        MANDRILL_API ~DescriptorDesc()
        {
        }
    };

    class Descriptor
    {
    public:
        MANDRILL_API Descriptor(ptr<Device> pDevice, const std::vector<DescriptorDesc>& desc,
                                VkDescriptorSetLayout layout, uint32_t copies = 1);
        MANDRILL_API ~Descriptor();

        MANDRILL_API VkDescriptorSet getSet(uint32_t index = 0) const
        {
            return mSets[index];
        }

    private:
        void allocate(const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout, uint32_t copies);

        ptr<Device> mpDevice;

        VkDescriptorPool mPool;
        std::vector<VkDescriptorSet> mSets;
    };
} // namespace Mandrill
