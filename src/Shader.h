#pragma once

#include "Common.h"

#include "Device.h"

namespace Mandrill
{
    struct ShaderCreator {
        std::filesystem::path filename;
        std::string entry;
        VkShaderStageFlagBits stageFlags;

        MANDRILL_API ShaderCreator(std::filesystem::path filename, std::string entry, VkShaderStageFlagBits stageFlags)
            : filename(filename), entry(entry), stageFlags(stageFlags)
        {
        }
    };

    class Shader
    {
    public:
        MANDRILL_API Shader(std::shared_ptr<Device> pDevice, const std::vector<ShaderCreator>& creator);
        MANDRILL_API ~Shader();

        std::vector<VkShaderModule> mModules;
        std::vector<VkPipelineShaderStageCreateInfo> mStages;

    private:


        std::shared_ptr<Device> mpDevice;

        std::vector<std::string> mEntries;
    };
} // namespace Mandrill