#pragma once

#include "Common.h"

#include "Device.h"

namespace Mandrill
{
    struct LayoutDesc {
        uint32_t set;
        uint32_t binding;
        VkDescriptorType type;
        VkShaderStageFlags stage;

        MANDRILL_API LayoutDesc(uint32_t set, uint32_t binding, VkDescriptorType type, VkShaderStageFlagBits stage)
            : set(set), binding(binding), type(type), stage(stage)
        {
        }
    };

    class Layout
    {
    public:
        MANDRILL_API
        Layout(ptr<Device> pDevice, const std::vector<LayoutDesc>& desc, VkDescriptorSetLayoutCreateFlags flags = 0);
        MANDRILL_API ~Layout();

        MANDRILL_API const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const
        {
            return mDescriptorSetLayouts;
        }

        MANDRILL_API const std::vector<VkPushConstantRange>& getPushConstantRanges() const
        {
            return mPushConstantRanges;
        }

        MANDRILL_API void addPushConstantRange(VkPushConstantRange pushConstantRange)
        {
            mPushConstantRanges.push_back(pushConstantRange);
        }

    private:
        ptr<Device> mpDevice;

        std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
        std::vector<VkPushConstantRange> mPushConstantRanges;
    };
} // namespace Mandrill
