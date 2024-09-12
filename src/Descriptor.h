#pragma once

#include "Common.h"

#include "Buffer.h"
#include "Device.h"
#include "Texture.h"

namespace Mandrill
{
    struct DescriptorDesc {
        VkDescriptorType type;
        union {
            std::shared_ptr<Buffer> pBuffer;
            std::shared_ptr<Image> pImage;
            std::shared_ptr<Texture> pTexture;
        };

        MANDRILL_API DescriptorDesc(VkDescriptorType type, std::shared_ptr<void> pResource) : type(type)
        {
            switch (type) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                pBuffer = std::static_pointer_cast<Buffer>(pResource);
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                pImage = std::static_pointer_cast<Image>(pResource);
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                pTexture = std::static_pointer_cast<Texture>(pResource);
                break;
            }
        }

        MANDRILL_API ~DescriptorDesc()
        {
        }
    };

    class Descriptor
    {
    public:
        MANDRILL_API Descriptor(std::shared_ptr<Device> pDevice, const std::vector<DescriptorDesc>& desc,
                                VkDescriptorSetLayout layout, uint32_t copies);
        MANDRILL_API ~Descriptor();

        MANDRILL_API VkDescriptorSet getSet(uint32_t index) const
        {
            return mSets[index];
        }

    private:
        void allocate(const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout, uint32_t copies);

        std::shared_ptr<Device> mpDevice;

        VkDescriptorPool mPool;
        std::vector<VkDescriptorSet> mSets;
    };
} // namespace Mandrill
