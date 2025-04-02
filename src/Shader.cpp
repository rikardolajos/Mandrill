#include "Shader.h"

#include "Error.h"
#include "Log.h"

using namespace Mandrill;

// Compile GLSL to SPIR-V
static bool compile(const std::filesystem::path& input, const std::filesystem::path& output)
{
    Log::debug("Compiling {}", input.string());

    auto depFile = output.parent_path() / output.stem();
    depFile += ".d";

#if MANDRILL_LINUX

    std::string cmd = std::format("glslc --target-env=vulkan1.3 -MD -MF {} {} -o {}", depFile.string(), input.string(),
                                  output.string());

#elif MANDRILL_WINDOWS

    const char* sdk = getenv("VULKAN_SDK");
    if (!sdk) {
        Log::error("VULKAN_SDK not found");
        return false;
    }
    std::string cmd = std::format("{}\\Bin\\glslc.exe --target-env=vulkan1.3 -MD -MF {} {} -o {}", sdk,
                                  depFile.string(), input.string(), output.string());

#else
#error "SHADER: Unsupported target platform"
#endif

    int res = std::system(cmd.c_str());

    if (res) {
        Log::error("Failed to compile {}", input.string());
    }

    return true;
}


// Find dependency file and compile
static void findDependenciesAndCompile(const std::filesystem::path& input)
{
    std::filesystem::path depFile = input;
    depFile += ".d";

    std::ifstream is(depFile, std::ios::binary);
    if (!is.is_open()) {
        Log::error("Unable to open {}", depFile.string());
        return;
    }

    std::string src;
    std::string dst;
    is >> dst;
    is >> src;

    // Remove trailing ":" from dst
    dst.pop_back();

    compile(src, dst);
}


Shader::Shader(ptr<Device> pDevice, const std::vector<ShaderDesc>& desc) : mpDevice(pDevice)
{
    mModules.resize(desc.size());
    mStages.resize(desc.size());
    mEntries.resize(desc.size());
    mSrcFilenames.resize(desc.size());
    mStageFlags.resize(desc.size());
    mSpecializationInfos.resize(desc.size());

    for (size_t i = 0; i < desc.size(); i++) {
        mSrcFilenames[i] = getExecutablePath() / desc[i].filename;
        mEntries[i] = desc[i].entry;
        mStageFlags[i] = desc[i].stageFlags;
        mSpecializationInfos[i] = desc[i].pSpecializationInfo;
    }

    createModulesAndStages();
}

Shader::~Shader()
{
    Check::Vk(vkDeviceWaitIdle(mpDevice->getDevice()));

    for (auto& m : mModules) {
        vkDestroyShaderModule(mpDevice->getDevice(), m, nullptr);
    }
}


void Shader::reload()
{
    for (size_t i = 0; i < mModules.size(); i++) {
        findDependenciesAndCompile(mSrcFilenames[i]);
    }

    Check::Vk(vkDeviceWaitIdle(mpDevice->getDevice()));

    for (auto& m : mModules) {
        vkDestroyShaderModule(mpDevice->getDevice(), m, nullptr);
    }

    createModulesAndStages();
}

void Shader::createModulesAndStages()
{
    for (size_t i = 0; i < mModules.size(); i++) {
        mModules[i] = loadModuleFromFile(mSrcFilenames[i]);
        mStages[i] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = mStageFlags[i],
            .module = mModules[i],
            .pName = mEntries[i].c_str(),
            .pSpecializationInfo = mSpecializationInfos[i],
        };
    }
}

VkShaderModule Shader::loadModuleFromFile(const std::filesystem::path& src)
{
    VkShaderModule module;

    std::filesystem::path dst = src;
    dst += ".spv";

    std::ifstream is(dst, std::ios::binary);
    if (!is.is_open()) {
        Log::error("Unable to open {}", dst.string());
    }

    is.seekg(0, std::ios_base::end);
    auto length = is.tellg();
    is.seekg(0, std::ios_base::beg);

    std::vector<uint32_t> buffer(length);

    is.read(reinterpret_cast<char*>(buffer.data()), length);

    is.close();

    VkShaderModuleCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = static_cast<size_t>(length),
        .pCode = buffer.data(),
    };

    Check::Vk(vkCreateShaderModule(mpDevice->getDevice(), &ci, nullptr, &module));

    return module;
}
