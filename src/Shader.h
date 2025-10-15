#pragma once

#include "Common.h"

#include "Descriptor.h"
#include "Device.h"

namespace Mandrill
{
    struct ShaderDesc {
        /// <summary>
        /// Path to shader source code
        /// </summary>
        std::filesystem::path filename;

        /// <summary>
        /// Entry point of shader (typically "main")
        /// </summary>
        std::string entry;

        /// <summary>
        /// Shader stage
        /// </summary>
        VkShaderStageFlagBits stageFlags;

        /// <summary>
        /// Optional specialization info constants
        /// </summary>
        VkSpecializationInfo* pSpecializationInfo;

        /// <summary>
        /// Shader description constructor, used to create a shader module.
        /// </summary>
        /// <param name="filename">Path to shader source code</param>
        /// <param name="entry">Entry point of shader (typically "main")</param>
        /// <param name="stageFlags">Shader stage</param>
        /// <param name="pSpecializationInfo">Optional setup for specialization constants</param>
        MANDRILL_API ShaderDesc(std::filesystem::path filename, std::string entry, VkShaderStageFlagBits stageFlags,
                                VkSpecializationInfo* pSpecializationInfo = nullptr)
            : filename(filename), entry(entry), stageFlags(stageFlags), pSpecializationInfo(pSpecializationInfo)
        {
        }
    };

    /// <summary>
    /// Shader class that abstracts the handling of Vulkan shaders. This class manages shader modules and hot reloading
    /// of shader source code during execution.
    /// </summary>
    class Shader
    {
    public:
        MANDRILL_NON_COPYABLE(Shader)

        /// <summary>
        /// Create a new shader.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="desc">Description of shader being created</param>
        MANDRILL_API Shader(ptr<Device> pDevice, const std::vector<ShaderDesc>& desc);

        /// <summary>
        /// Destructor for shader.
        /// </summary>
        MANDRILL_API ~Shader();

        /// <summary>
        /// Reload shader code from disk and recompile it.
        /// </summary>
        MANDRILL_API void reload();

        MANDRILL_API void bind(VkCommandBuffer cmd, ptr<Descriptor> pDescriptor, uint32_t set);

        /// <summary>
        /// Get shader module handles.
        /// </summary>
        /// <returns>Vector of shader module handles</returns>
        MANDRILL_API std::vector<VkShaderModule> getModules() const
        {
            return mModules;
        }

        /// <summary>
        /// Get pipeline shader stage create infos.
        /// </summary>
        /// <returns>Vector of pipeline shader stage create infos</returns>
        MANDRILL_API std::vector<VkPipelineShaderStageCreateInfo> getStages() const
        {
            return mStages;
        }

        /// <summary>
        /// Get the descriptor set layout of one set.
        /// </summary>
        /// <returns>Vector of descriptor set layouts</returns>
        MANDRILL_API VkDescriptorSetLayout getDescriptorSetLayout(uint32_t set) const
        {
            return mDescriptorSetLayouts[set];
        }

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
        /// Get tthe pipeline layout.
        /// </summary>
        /// <returns>Pipeline layout</returns>
        MANDRILL_API VkPipelineLayout getPipelineLayout() const
        {
            return mPipelineLayout;
        }

    private:
        void createShader();

        ptr<Device> mpDevice;

        std::vector<VkShaderModule> mModules;
        std::vector<ptr<spv_reflect::ShaderModule>> mReflections;
        std::vector<VkPipelineShaderStageCreateInfo> mStages;

        std::vector<std::string> mEntries;
        std::vector<std::filesystem::path> mSrcFilenames;
        std::vector<VkShaderStageFlagBits> mStageFlags;
        std::vector<VkSpecializationInfo*> mSpecializationInfos;

        std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts; // One per set
        std::vector<VkPushConstantRange> mPushConstantRanges;
        VkPipelineLayout mPipelineLayout;
    };
} // namespace Mandrill
