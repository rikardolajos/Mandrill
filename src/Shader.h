#pragma once

#include "Common.h"

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

    private:
        void createModulesAndStages();
        VkShaderModule loadModuleFromFile(const std::filesystem::path& input);

        ptr<Device> mpDevice;

        std::vector<VkShaderModule> mModules;
        std::vector<VkPipelineShaderStageCreateInfo> mStages;

        std::vector<std::string> mEntries;
        std::vector<std::filesystem::path> mSrcFilenames;
        std::vector<VkShaderStageFlagBits> mStageFlags;
        std::vector<VkSpecializationInfo*> mSpecializationInfos;
    };
} // namespace Mandrill
