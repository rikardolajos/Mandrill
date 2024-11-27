#include "Descriptor.h"

#include "AccelerationStructure.h"
#include "Error.h"
#include "Helpers.h"

using namespace Mandrill;

Descriptor::Descriptor(ptr<Device> pDevice, const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout,
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
            VkWriteDescriptorSetAccelerationStructureKHR asi;
            VkAccelerationStructureKHR as;
            VkDeviceSize offset;

            switch (desc[d].type) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                offset =
                    Helpers::alignTo(i * (std::get<ptr<Buffer>>(desc[d].pResource)->getSize() / copies),
                                     mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment);
                bi = {
                    .buffer = std::get<ptr<Buffer>>(desc[d].pResource)->getBuffer(),
                    .offset = desc[d].offset ? desc[d].offset : offset,
                    .range =
                        desc[d].range ? desc[d].range : std::get<ptr<Buffer>>(desc[d].pResource)->getSize() / copies,
                };
                write.pBufferInfo = &bi;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                offset =
                    Helpers::alignTo(i * (std::get<ptr<Buffer>>(desc[d].pResource)->getSize() / copies),
                                     mpDevice->getProperties().physicalDevice.limits.minStorageBufferOffsetAlignment);
                bi = {
                    .buffer = std::get<ptr<Buffer>>(desc[d].pResource)->getBuffer(),
                    .offset = desc[d].offset ? desc[d].offset : offset,
                    .range =
                        desc[d].range ? desc[d].range : std::get<ptr<Buffer>>(desc[d].pResource)->getSize() / copies,
                };
                write.pBufferInfo = &bi;
                break;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                ii = {
                    .sampler = std::get<ptr<Texture>>(desc[d].pResource)->getSampler(),
                    .imageView = desc[d].imageView ? desc[d].imageView
                                                   : std::get<ptr<Texture>>(desc[d].pResource)->getImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                write.pImageInfo = &ii;
                break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                ii = {
                    .imageView =
                        desc[d].imageView ? desc[d].imageView : std::get<ptr<Image>>(desc[d].pResource)->getImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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

    // Set layout (same for all copies)
    std::vector<VkDescriptorSetLayout> layouts(copies, layout);

    VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };

    mSets.resize(copies);
    Check::Vk(vkAllocateDescriptorSets(mpDevice->getDevice(), &ai, mSets.data()));
}
