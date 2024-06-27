#pragma once

#include "Common.h"

#include "Device.h"

namespace Mandrill
{
    struct ShaderDescription {
        std::filesystem::path filename;
        std::string entry;
        VkShaderStageFlagBits stageFlags;

        MANDRILL_API ShaderDescription(std::filesystem::path filename, std::string entry, VkShaderStageFlagBits stageFlags)
            : filename(filename), entry(entry), stageFlags(stageFlags)
        {
        }
    };

    class Shader
    {
    public:
        MANDRILL_API Shader(std::shared_ptr<Device> pDevice, const std::vector<ShaderDescription>& desc);
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

        std::shared_ptr<Device> mpDevice;

        std::vector<VkShaderModule> mModules;
        std::vector<VkPipelineShaderStageCreateInfo> mStages;

        std::vector<std::string> mEntries;
        std::vector<std::filesystem::path> mSrcFilenames;
        std::vector<VkShaderStageFlagBits> mStageFlags;
    };
} // namespace Mandrill