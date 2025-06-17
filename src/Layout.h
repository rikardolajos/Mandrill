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
        uint32_t arrayCount;

        /// <summary>
        /// Layout description constructor, used to create a layout.
        /// </summary>
        /// <param name="set">Desctiptor set</param>
        /// <param name="binding">Set binding</param>
        /// <param name="type">Descriptor tpye</param>
        /// <param name="stage">Shader stage</param>
        /// <param name="arrayCount">If descriptor array, array length</param>
        /// <returns></returns>
        MANDRILL_API LayoutDesc(uint32_t set, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage,
                                uint32_t arrayCount = 0)
            : set(set), binding(binding), type(type), stage(stage), arrayCount(arrayCount)
        {
        }
    };

    /// <summary>
    /// Layout class that abstracts the handling of descriptor set layouts and push constants in Vulkan.
    /// </summary>
    class Layout
    {
    public:
        MANDRILL_NON_COPYABLE(Layout)

        /// <summary>
        /// Create a new layout.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="desc">Description of the layout</param>
        /// <param name="flags">Creation flags</param>
        MANDRILL_API
        Layout(ptr<Device> pDevice, const std::vector<LayoutDesc>& desc, VkDescriptorSetLayoutCreateFlags flags = 0);

        /// <summary>
        /// Destructor for layout.
        /// </summary>
        MANDRILL_API ~Layout();

        /// <summary>
        /// Get the descriptor set layouts.
        /// </summary>
        /// <returns>Vector of descriptor set layouts</returns>
        MANDRILL_API const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const
        {
            return mDescriptorSetLayouts;
        }

        /// <summary>
        /// Get the push constant ranges.
        /// </summary>
        /// <returns>Vector of push constant ranges</returns>
        MANDRILL_API const std::vector<VkPushConstantRange>& getPushConstantRanges() const
        {
            return mPushConstantRanges;
        }

        /// <summary>
        /// Add a new push constant range.
        /// </summary>
        /// <param name="pushConstantRange">Push constant range to add</param>
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
