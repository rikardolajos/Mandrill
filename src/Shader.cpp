#include "shader.h"

#include "Error.h"
#include "Log.h"

using namespace Mandrill;

#ifdef _DEBUG
// Compile GLSL to SPIR-V
static bool compile(const std::filesystem::path& input, const std::filesystem::path& output)
{
    Log::debug("Compiling {}", input.string());

#if MANDRILL_LINUX

    std::string cmd = std::format("glslc --target-env=vulkan1.3 {} -o {}", input.string(), output.string());

#elif MANDRILL_WINDOWS

    const char* sdk = getenv("VULKAN_SDK");
    if (!sdk) {
        Log::error("VULKAN_SDK not found");
        return false;
    }
    std::string cmd =
        std::format("{}\\Bin\\glslc.exe --target-env=vulkan1.3 {} -o {}", sdk, input.string(), output.string());

#else
#error "SHADER: Unsupported target platform"
#endif

    int res = std::system(cmd.c_str());

    if (res) {
        Log::error("Failed to compile {}", input.string());
    }

    return true;
}
#endif


// Load shader module from pre-compiled SPIR-V binary
static VkShaderModule load(VkDevice device, const std::filesystem::path& input)
{
    VkShaderModule module;

    std::ifstream is(input, std::ios::binary);
    if (!is.is_open()) {
        Log::error("Unable to open {}", input.string());
    }

    is.seekg(0, std::ios_base::end);
    auto length = is.tellg();
    is.seekg(0, std::ios_base::beg);

    std::vector<uint32_t> buffer(length);

    is.read(reinterpret_cast<char*>(buffer.data()), length);

    is.close();

    VkShaderModuleCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = static_cast<size_t>(length),
        .pCode = buffer.data(),
    };

    Check::Vk(vkCreateShaderModule(device, &ci, nullptr, &module));

    return module;
}


// Either load pre-compiled SPIR-V or compile and then load
static VkShaderModule moduleFromFile(VkDevice device, const std::filesystem::path& input)
{
    std::filesystem::path output = input;
    output += ".spv";

#ifdef _DEBUG
    compile(input, output);
#endif
    return load(device, output);
}


Shader::Shader(std::shared_ptr<Device> pDevice, const std::vector<ShaderCreator>& creator) : mpDevice(pDevice)
{
    mModules.resize(creator.size());
    mEntries.resize(creator.size());
    mStages.resize(creator.size());

    for (int i = 0; i < creator.size(); i++) {
        mModules[i] = moduleFromFile(mpDevice->getDevice(), creator[i].filename);
        mEntries[i] = creator[i].entry;
        mStages[i] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = creator[i].stageFlags,
            .module = mModules[i],
            .pName = mEntries[i].c_str(),
        };
    }
}

Shader::~Shader()
{
    Check::Vk(vkDeviceWaitIdle(mpDevice->getDevice()));

    for (auto& m : mModules) {
        vkDestroyShaderModule(mpDevice->getDevice(), m, nullptr);
    }
}
