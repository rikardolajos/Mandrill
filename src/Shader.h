#pragma once

#include "Common.h"

#include "Device.h"

namespace Mandrill
{
    struct ShaderDesc {
        std::filesystem::path filename;
        std::string entry;
        VkShaderStageFlagBits stageFlags;
        VkSpecializationInfo* pSpecializationInfo;

        MANDRILL_API ShaderDesc(std::filesystem::path filename, std::string entry, VkShaderStageFlagBits stageFlags,
                                VkSpecializationInfo* pSpecializationInfo = nullptr)
            : filename(filename), entry(entry), stageFlags(stageFlags), pSpecializationInfo(pSpecializationInfo)
        {
        }
    };

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
        /// <returns></returns>
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
