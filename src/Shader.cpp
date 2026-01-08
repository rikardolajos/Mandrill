#include "Shader.h"

#include "Error.h"
#include "Log.h"

using namespace Mandrill;

class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
{
    static std::string ReadFile(const std::string& filepath)
    {
        std::string sourceCode;
        std::ifstream in(filepath, std::ios::in | std::ios::binary);
        if (in) {
            in.seekg(0, std::ios::end);
            size_t size = in.tellg();
            if (size > 0) {
                sourceCode.resize(size);
                in.seekg(0, std::ios::beg);
                in.read(&sourceCode[0], size);
            } else {
                Log::Warning("ShaderIncluder::ReadFile: Could not read shader file '{0}'", filepath);
            }
        } else {
            Log::Warning("ShaderIncluder::ReadFile Could not open shader file '{0}'", filepath);
        }
        return sourceCode;
    }

    shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type,
                                       const char* requestingSource, size_t includeDepth) override
    {
        // Construct full path of requested include
        std::filesystem::path srcPath(requestingSource);
        std::filesystem::path incPath = srcPath.parent_path() / std::filesystem::path(requestedSource);

        const std::string name = std::string(requestedSource);
        const std::string contents = ReadFile(incPath.string());

        auto container = new std::array<std::string, 2>;
        (*container)[0] = name;
        (*container)[1] = contents;

        auto data = new shaderc_include_result;

        data->user_data = container;

        data->source_name = (*container)[0].data();
        data->source_name_length = (*container)[0].size();

        data->content = (*container)[1].data();
        data->content_length = (*container)[1].size();

        return data;
    }

    void ReleaseInclude(shaderc_include_result* data) override
    {
        delete static_cast<std::array<std::string, 2>*>(data->user_data);
        delete data;
    }
};

// Compile GLSL to SPIR-V
static bool compile(const std::filesystem::path& input, const std::filesystem::path& output,
                    VkShaderStageFlagBits stageFlag)
{
    Log::Debug("Compiling {}", input.string());

    auto depFile = output.parent_path() / output.stem();
    depFile += ".d";

    std::ifstream file(input, std::ios::binary);
    if (!file.is_open()) {
        Log::Error("Unable to open {}", input.string());
        return false;
    }
    std::string shaderSourceStr;
    {
        std::ostringstream ss;
        ss << file.rdbuf();
        shaderSourceStr = ss.str();
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
    options.SetTargetSpirv(shaderc_spirv_version_1_6);
    options.SetIncluder(std::make_unique<ShaderIncluder>());
#ifdef _DEBUG
    options.SetGenerateDebugInfo();
    options.SetWarningsAsErrors();
#else
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif
    options.SetOptimizationLevel(shaderc_optimization_level_zero);

    shaderc_shader_kind kind = shaderc_glsl_infer_from_source;
    switch (stageFlag) {
    case VK_SHADER_STAGE_VERTEX_BIT:
        kind = shaderc_vertex_shader;
        break;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        kind = shaderc_tess_control_shader;
        break;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        kind = shaderc_tess_evaluation_shader;
        break;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        kind = shaderc_geometry_shader;
        break;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        kind = shaderc_fragment_shader;
        break;
    case VK_SHADER_STAGE_COMPUTE_BIT:
        kind = shaderc_compute_shader;
        break;
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        kind = shaderc_raygen_shader;
        break;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        kind = shaderc_anyhit_shader;
        break;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        kind = shaderc_closesthit_shader;
        break;
    case VK_SHADER_STAGE_MISS_BIT_KHR:
        kind = shaderc_miss_shader;
        break;
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        kind = shaderc_intersection_shader;
        break;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        kind = shaderc_callable_shader;
        break;
    case VK_SHADER_STAGE_TASK_BIT_EXT:
        kind = shaderc_task_shader;
        break;
    case VK_SHADER_STAGE_MESH_BIT_EXT:
        kind = shaderc_mesh_shader;
        break;
    }

    shaderc::CompilationResult results =
        compiler.CompileGlslToSpv(shaderSourceStr, kind, input.string().c_str(), options);

    if (results.GetCompilationStatus() != shaderc_compilation_status_success) {
        Log::Error("{}", results.GetErrorMessage());
        return false;
    }

    std::vector<uint32_t> code = {results.cbegin(), results.cend()};

    // Write SPIR-V to file
    std::ofstream os(output, std::ios::binary);
    if (!os.is_open()) {
        Log::Error("Unable to open {}", output.string());
        return false;
    }
    os.write(reinterpret_cast<const char*>(code.data()), code.size() * sizeof(uint32_t));

    return true;
}

// Find dependency file and compile
static void findDependenciesAndCompile(const std::filesystem::path& input, VkShaderStageFlagBits stageFlag)
{
    std::filesystem::path depFile = input;
    depFile += ".d";

    std::ifstream is(depFile, std::ios::binary);
    if (!is.is_open()) {
        Log::Error("Unable to open {}", depFile.string());
        return;
    }

    std::string src;
    std::string dst;
    is >> dst;
    is >> src;

    // Remove trailing ":" from dst
    dst.pop_back();

    compile(src, dst, stageFlag);
}

// Read SPIR-V from file
static std::vector<uint32_t> readSpirv(const std::filesystem::path& src)
{
    std::filesystem::path dst = src;
    dst += ".spv";

    std::ifstream is(dst, std::ios::binary);
    if (!is.is_open()) {
        Log::Error("Unable to open {}", dst.string());
    }

    is.seekg(0, std::ios_base::end);
    auto length = is.tellg();
    is.seekg(0, std::ios_base::beg);

    std::vector<uint32_t> buffer(length);

    is.read(reinterpret_cast<char*>(buffer.data()), length);
    is.close();

    return buffer;
}

static VkShaderModule createModule(ptr<Device> pDevice, std::vector<uint32_t> spirvBytes)
{
    VkShaderModule module;

    VkShaderModuleCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirvBytes.size(),
        .pCode = spirvBytes.data(),
    };

    Check::Vk(vkCreateShaderModule(pDevice->getDevice(), &ci, nullptr, &module));

    return module;
}

static ptr<spv_reflect::ShaderModule> createReflections(std::vector<uint32_t> spirvBytes)
{
    spv_reflect::ShaderModule moduleReflect(spirvBytes.size(), spirvBytes.data());
    return make_ptr<spv_reflect::ShaderModule>(std::move(moduleReflect));
}

// Should only be used for uint32_t specialization constants to get the array length
static uint32_t getSpecializationConstant(const std::vector<VkSpecializationInfo*>& specializationInfos,
                                          uint32_t constantId)
{
    uint32_t value = 0xffffffff;
    for (VkSpecializationInfo* pInfo : specializationInfos) {
        if (pInfo == nullptr) {
            continue;
        }
        for (uint32_t i = 0; i < pInfo->mapEntryCount; i++) {
            const VkSpecializationMapEntry* entry = &pInfo->pMapEntries[i];
            if (entry->constantID == constantId) {
                const uint8_t* pData = static_cast<const uint8_t*>(pInfo->pData);
                std::memcpy(&value, pData + entry->offset, entry->size);
                return value;
            }
        }
    }
    return value;
}

Shader::Shader(ptr<Device> pDevice, const std::vector<ShaderDesc>& desc) : mpDevice(pDevice)
{
    mModules.resize(desc.size());
    mReflections.resize(desc.size());
    mStages.resize(desc.size());
    mEntries.resize(desc.size());
    mSrcFilenames.resize(desc.size());
    mStageFlags.resize(desc.size());
    mSpecializationInfos.resize(desc.size());

    for (size_t i = 0; i < desc.size(); i++) {
        mSrcFilenames[i] = GetExecutablePath() / desc[i].filename;
        mEntries[i] = desc[i].entry;
        mStageFlags[i] = desc[i].stageFlags;
        mSpecializationInfos[i] = desc[i].pSpecializationInfo;
    }

    createShader();
}

Shader::~Shader()
{
    Check::Vk(vkDeviceWaitIdle(mpDevice->getDevice()));

    vkDestroyPipelineLayout(mpDevice->getDevice(), mPipelineLayout, nullptr);

    for (auto& l : mDescriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(mpDevice->getDevice(), l, nullptr);
    }

    for (auto& m : mModules) {
        vkDestroyShaderModule(mpDevice->getDevice(), m, nullptr);
    }
}

void Shader::reload()
{
    for (size_t i = 0; i < mModules.size(); i++) {
        findDependenciesAndCompile(mSrcFilenames[i], mStageFlags[i]);
    }

    Check::Vk(vkDeviceWaitIdle(mpDevice->getDevice()));

    vkDestroyPipelineLayout(mpDevice->getDevice(), mPipelineLayout, nullptr);

    for (auto& l : mDescriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(mpDevice->getDevice(), l, nullptr);
    }

    for (auto& m : mModules) {
        vkDestroyShaderModule(mpDevice->getDevice(), m, nullptr);
    }

    createShader();
}

void Shader::createShader()
{
    // Load modules
    for (size_t i = 0; i < mModules.size(); i++) {
        auto spirvBytes = readSpirv(mSrcFilenames[i]);
        mModules[i] = createModule(mpDevice, spirvBytes);
        mReflections[i] = createReflections(spirvBytes);
        mStages[i] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = mStageFlags[i],
            .module = mModules[i],
            .pName = mEntries[i].c_str(),
            .pSpecializationInfo = mSpecializationInfos[i],
        };
    }

    // Collect descriptor set layouts
    std::vector<std::map<uint32_t, SpvReflectDescriptorBinding*>> descriptorSets;
    std::vector<VkShaderStageFlags> stageFlags;
    std::vector<std::vector<uint32_t>> bindingCount;
    for (size_t i = 0; i < mReflections.size(); i++) {
        uint32_t bindingsCount = 0;
        mReflections[i]->EnumerateDescriptorBindings(&bindingsCount, nullptr);
        std::vector<SpvReflectDescriptorBinding*> bindings(bindingsCount);
        mReflections[i]->EnumerateDescriptorBindings(&bindingsCount, bindings.data());

        uint32_t maxSet = 0;
        for (auto& binding : bindings) {
            maxSet = std::max(maxSet, binding->set);
        }
        descriptorSets.resize(std::max(descriptorSets.size(), static_cast<size_t>(maxSet + 1)));
        stageFlags.resize(std::max(stageFlags.size(), static_cast<size_t>(maxSet + 1)));
        bindingCount.resize(std::max(bindingCount.size(), static_cast<size_t>(maxSet + 1)));

        // Resolve specialization constants (if used for array dimensions)
        for (auto& binding : bindings) {
            if (binding->array.dims_count != 0) {
            }
        }
        uint32_t specConstCount = 0;
        mReflections[i]->EnumerateSpecializationConstants(&specConstCount, nullptr);
        std::vector<SpvReflectSpecializationConstant*> specConsts(specConstCount);
        mReflections[i]->EnumerateSpecializationConstants(&specConstCount, specConsts.data());
        mSpecializationInfos[i]->pMapEntries->constantID;

        for (auto& binding : bindings) {
            descriptorSets[binding->set].emplace(binding->binding, binding);
            stageFlags[binding->set] |= static_cast<VkShaderStageFlagBits>(mReflections[i]->GetShaderStage());

            if (binding->array.dims_count == 0) {
                bindingCount[binding->set].push_back(binding->count);
            } else if (binding->array.dims_count == 1) {
                if (binding->type_description->traits.array.spec_constant_op_ids[0] != 0xffffffff) {
                    uint32_t id = binding->type_description->traits.array.spec_constant_op_ids[0];
                    bindingCount[binding->set].push_back(getSpecializationConstant(mSpecializationInfos, id));
                } else if (binding->array.dims[0] > 0) {
                    bindingCount[binding->set].push_back(binding->array.dims[0]);
                } else if (binding->array.dims[0] == 0) {
                    Log::Error("Unsized array: cannot determine descriptor set layout. Use sized arrays or "
                               "specialization constants.");
                }
            } else if (binding->array.dims_count > 1) {
                Log::Error("Array descriptors with more than 1 dimension is not supported.");
            }
        }
    }

    auto getType = [](SpvReflectDescriptorBinding* binding) -> VkDescriptorType {
        if (binding->block.type_description) {
            std::string typeName(binding->block.type_description->type_name);
            if (typeName.ends_with("Dynamic")) {
                switch (binding->descriptor_type) {
                case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                }
            }
        }
        return static_cast<VkDescriptorType>(binding->descriptor_type);
    };

    // Create descriptor set layouts
    mDescriptorSetLayouts.clear();
    mDescriptorSetLayouts.resize(descriptorSets.size());
    for (uint32_t i = 0; i < count(descriptorSets); i++) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (uint32_t j = 0; j < count(descriptorSets[i]); j++) {
            bindings.push_back({
                .binding = descriptorSets[i][j]->binding,
                .descriptorType = getType(descriptorSets[i][j]),
                .descriptorCount = bindingCount[i][j],
                .stageFlags = stageFlags[i],
            });
        }

        VkDescriptorSetLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = count(bindings),
            .pBindings = bindings.data(),
        };

        Check::Vk(vkCreateDescriptorSetLayout(mpDevice->getDevice(), &ci, nullptr, &mDescriptorSetLayouts[i]));
    }

    // Create push constant ranges
    mPushConstantRanges.clear();
    for (size_t i = 0; i < mReflections.size(); i++) {
        uint32_t pushConstCount = 0;
        mReflections[i]->EnumeratePushConstantBlocks(&pushConstCount, nullptr);
        std::vector<SpvReflectBlockVariable*> pushConsts(pushConstCount);
        mReflections[i]->EnumeratePushConstantBlocks(&pushConstCount, pushConsts.data());
        for (auto& p : pushConsts) {
            VkPushConstantRange range = {
                .stageFlags = static_cast<VkShaderStageFlags>(mReflections[i]->GetShaderStage()),
                .offset = p->offset,
                .size = p->size,
            };
            mPushConstantRanges.push_back(range);
        }
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = count(mDescriptorSetLayouts),
        .pSetLayouts = mDescriptorSetLayouts.data(),
        .pushConstantRangeCount = count(mPushConstantRanges),
        .pPushConstantRanges = mPushConstantRanges.data(),
    };

    Check::Vk(vkCreatePipelineLayout(mpDevice->getDevice(), &ci, nullptr, &mPipelineLayout));
}
