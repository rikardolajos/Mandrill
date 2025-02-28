#include "Descriptor.h"

#include "AccelerationStructure.h"
#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

Descriptor::Descriptor(ptr<Device> pDevice, const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout)
    : mpDevice(pDevice)
{
    allocate(desc, layout);

    for (uint32_t d = 0; d < count(desc); d++) {
        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = mSet,
            .dstBinding = d,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = desc[d].type,
        };

        VkDescriptorBufferInfo bi;
        VkDescriptorImageInfo ii;
        std::vector<VkDescriptorImageInfo> iis;
        VkWriteDescriptorSetAccelerationStructureKHR asi;
        VkAccelerationStructureKHR as;

        switch (desc[d].type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            bi = {
                .buffer = std::get<ptr<Buffer>>(desc[d].pResource)->getBuffer(),
                .offset = desc[d].offset,
                .range = desc[d].range,
            };
            write.pBufferInfo = &bi;
            break;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            if (desc[d].arrayCount) {
                auto textures = std::get<ptr<std::vector<ptr<Texture>>>>(desc[d].pResource);
                iis.resize(desc[d].arrayCount);
                for (uint32_t t = 0; t < desc[d].arrayCount; t++) {
                    iis[t] = {
                        .sampler = textures->at(t)->getSampler(),
                        .imageView = textures->at(t)->getImageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    };
                }
                write.descriptorCount = static_cast<uint32_t>(desc[d].arrayCount);
            } else {
                iis.resize(1);
                iis[0] = {
                    .sampler = std::get<ptr<Texture>>(desc[d].pResource)->getSampler(),
                    .imageView = desc[d].imageView ? desc[d].imageView
                                                   : std::get<ptr<Texture>>(desc[d].pResource)->getImageView(),
                    .imageLayout = desc[d].imageLayout ? desc[d].imageLayout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
            }
            write.pImageInfo = iis.data();
            break;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            ii = {
                .imageView =
                    desc[d].imageView ? desc[d].imageView : std::get<ptr<Image>>(desc[d].pResource)->getImageView(),
                .imageLayout = desc[d].imageLayout ? desc[d].imageLayout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            write.pImageInfo = &ii;
            break;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            as = std::get<ptr<AccelerationStructure>>(desc[d].pResource)->getAccelerationStructure();
            asi = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &as,
            };
            write.pNext = &asi;
            break;
        }
        vkUpdateDescriptorSets(mpDevice->getDevice(), 1, &write, 0, nullptr);
    }
}

Descriptor::~Descriptor()
{
    Check::Vk(vkDeviceWaitIdle(mpDevice->getDevice()));

    // Allocated descriptor sets are implicitly freed
    vkDestroyDescriptorPool(mpDevice->getDevice(), mPool, nullptr);
}

void Descriptor::allocate(const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout)
{
    // Pool
    std::vector<VkDescriptorPoolSize> poolSizes(desc.size());
    for (uint32_t i = 0; i < count(poolSizes); i++) {
        poolSizes[i].type = desc[i].type;
        poolSizes[i].descriptorCount = desc[i].arrayCount ? desc[i].arrayCount : 1;
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = count(poolSizes),
        .pPoolSizes = poolSizes.data(),
    };

    Check::Vk(vkCreateDescriptorPool(mpDevice->getDevice(), &poolInfo, nullptr, &mPool));

    // Set layout
    VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    Check::Vk(vkAllocateDescriptorSets(mpDevice->getDevice(), &ai, &mSet));
}
