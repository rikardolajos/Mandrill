#include "Descriptor.h"

#include "Error.h"

using namespace Mandrill;

Descriptor::Descriptor(std::shared_ptr<Device> pDevice,
                       const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout,
                       uint32_t copies)
    : mpDevice(pDevice)
{
    allocate(desc, layout, copies);

    for (uint32_t d = 0; d < desc.size(); d++) {

        VkWriteDescriptorSet write;

        for (uint32_t i = 0; i < copies; i++) {
            write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = mSets[i],
                .dstBinding = d,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = desc[d].type,
            };

            VkDescriptorBufferInfo bi;
            VkDescriptorImageInfo ii;
            // VkWriteDescriptorSetAccelerationStructureKHR asi;

            switch (desc[d].type) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                bi = {
                    .buffer = desc[d].pBuffer->getBuffer(),
                    .offset = i * (desc[d].pBuffer->getSize() / copies),
                    .range = desc[d].pBuffer->getSize(),
                };
                write.pBufferInfo = &bi;
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                ii = {
                    .sampler = desc[d].pTexture->getSampler(),
                    .imageView = desc[d].pTexture->getImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                write.pImageInfo = &ii;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                ii = {
                    .imageView = desc[d].pImage->getImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                };
                write.pImageInfo = &ii;
                break;
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                // asi = {
                //     .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                //     .accelerationStructureCount = 1,
                //     .pAccelerationStructures = desc[d].pAccelerationStructure.get(),
                // };
                // writes.pNext = &accStructInfos[d];
                break;
            }
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

void Descriptor::allocate(const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout, uint32_t copies)
{
    // Pool
    std::vector<VkDescriptorPoolSize> poolSizes(desc.size());
    for (uint32_t i = 0; i < poolSizes.size(); i++) {
        poolSizes[i].type = desc[i].type;
        poolSizes[i].descriptorCount = copies;
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = copies,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    Check::Vk(vkCreateDescriptorPool(mpDevice->getDevice(), &poolInfo, nullptr, &mPool));

    // Set layout
    std::vector<VkDescriptorSetLayout> layouts(copies, layout);

    //// Same layout should be used for all descriptor sets
    // for (uint32_t i = 0; i < copies; i++) {
    //     std::memcpy(&layouts[i], &layout, sizeof(VkDescriptorSetLayout));
    // }

    VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };

    mSets.resize(copies);
    Check::Vk(vkAllocateDescriptorSets(mpDevice->getDevice(), &ai, mSets.data()));
}
