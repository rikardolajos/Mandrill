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
        MANDRILL_API Shader(ptr<Device> pDevice, const std::vector<ShaderDesc>& desc);
        MANDRILL_API ~Shader();

        MANDRILL_API void reload();

        MANDRILL_API std::vector<VkShaderModule> getModules() const
        {
            return mModules;
        }

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
