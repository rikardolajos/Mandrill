#include "Shader.h"

#include "Error.h"
#include "Log.h"

#include <deque>

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
    options.SetOptimizationLevel(shaderc_optimization_level_zero);
#else
    // Debug information is kept even when optimizing, because the descriptor reflection identifies dynamic buffers
    // by their block type name and the optimizer strips those names away otherwise
    options.SetGenerateDebugInfo();
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif

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

// Read SPIR-V from file. The returned vector holds words, so its size() is a word count, not a byte count.
static std::vector<uint32_t> readSpirv(const std::filesystem::path& src)
{
    std::filesystem::path dst = src;
    dst += ".spv";

    std::ifstream is(dst, std::ios::binary);
    if (!is.is_open()) {
        Log::Error("Unable to open {}", dst.string());
        return {};
    }

    is.seekg(0, std::ios_base::end);
    std::streamoff length = is.tellg();
    is.seekg(0, std::ios_base::beg);

    if (length <= 0 || length % sizeof(uint32_t) != 0) {
        Log::Error("{} is not a valid SPIR-V module ({} bytes)", dst.string(), length);
        return {};
    }

    std::vector<uint32_t> buffer(static_cast<size_t>(length) / sizeof(uint32_t));
    is.read(reinterpret_cast<char*>(buffer.data()), length);

    return buffer;
}

static VkShaderModule createModule(ptr<Device> pDevice, const std::vector<uint32_t>& spirv)
{
    VkShaderModule module;

    VkShaderModuleCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv.size() * sizeof(uint32_t),
        .pCode = spirv.data(),
    };

    Check::Vk(vkCreateShaderModule(pDevice->getDevice(), &ci, nullptr, &module));

    return module;
}

static ptr<spv_reflect::ShaderModule> createReflections(const std::vector<uint32_t>& spirv)
{
    spv_reflect::ShaderModule moduleReflect(spirv.size() * sizeof(uint32_t), spirv.data());
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

    destroyDescriptorPools();

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

    // The sets were allocated against layouts that are about to be destroyed. The attached resources are kept and
    // will be written into freshly allocated sets on the next bind.
    destroyDescriptorPools();

    vkDestroyPipelineLayout(mpDevice->getDevice(), mPipelineLayout, nullptr);

    for (auto& l : mDescriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(mpDevice->getDevice(), l, nullptr);
    }

    for (auto& m : mModules) {
        vkDestroyShaderModule(mpDevice->getDevice(), m, nullptr);
    }

    createShader();
}

void Shader::destroyDescriptorPools()
{
    for (auto& pool : mDescriptorPools) {
        vkDestroyDescriptorPool(mpDevice->getDevice(), pool, nullptr);
    }
    mDescriptorPools.clear();

    std::fill(mDescriptorSets.begin(), mDescriptorSets.end(), VK_NULL_HANDLE);
    std::fill(mSetDirty.begin(), mSetDirty.end(), true);
}

void Shader::createShader()
{
    // Load modules
    for (size_t i = 0; i < mModules.size(); i++) {
        auto spirv = readSpirv(mSrcFilenames[i]);
        if (spirv.empty()) {
            // Leave the stage empty rather than feeding an invalid module to the pipeline. Clearing matters on
            // reload(), where these still hold the previous load's module and reflection.
            Log::Error("Skipping shader stage {}, no SPIR-V was loaded", mSrcFilenames[i].string());
            mModules[i] = VK_NULL_HANDLE;
            mReflections[i].reset();
            mStages[i] = {};
            continue;
        }
        mModules[i] = createModule(mpDevice, spirv);
        mReflections[i] = createReflections(spirv);
        mStages[i] = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = mStageFlags[i],
            .module = mModules[i],
            .pName = mEntries[i].c_str(),
            .pSpecializationInfo = mSpecializationInfos[i],
        };
    }

    // Collect descriptor set layouts. Both maps are indexed by set, and then keyed on the binding number. Binding
    // numbers are not necessarily contiguous, so they cannot be used as indices into a vector.
    std::vector<std::map<uint32_t, SpvReflectDescriptorBinding*>> descriptorSets;
    std::vector<VkShaderStageFlags> stageFlags;
    std::vector<std::map<uint32_t, uint32_t>> bindingCount;
    for (size_t i = 0; i < mReflections.size(); i++) {
        if (!mReflections[i]) {
            continue; // Stage failed to load, already reported above
        }

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

        uint32_t specConstCount = 0;
        mReflections[i]->EnumerateSpecializationConstants(&specConstCount, nullptr);
        std::vector<SpvReflectSpecializationConstant*> specConsts(specConstCount);
        mReflections[i]->EnumerateSpecializationConstants(&specConstCount, specConsts.data());

        for (auto& binding : bindings) {
            descriptorSets[binding->set].emplace(binding->binding, binding);
            stageFlags[binding->set] |= static_cast<VkShaderStageFlagBits>(mReflections[i]->GetShaderStage());

            if (binding->array.dims_count == 0) {
                bindingCount[binding->set].insert_or_assign(binding->binding, binding->count);
            } else if (binding->array.dims_count == 1) {
                if (binding->type_description->traits.array.spec_constant_op_ids[0] != 0xffffffff) {
                    uint32_t id = binding->type_description->traits.array.spec_constant_op_ids[0];
                    bindingCount[binding->set].insert_or_assign(binding->binding,
                                                                getSpecializationConstant(mSpecializationInfos, id));
                } else if (binding->array.dims[0] > 0) {
                    bindingCount[binding->set].insert_or_assign(binding->binding, binding->array.dims[0]);
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
        // Block type names only survive if the SPIR-V kept its debug information. Without them the *Dynamic naming
        // convention cannot be detected, so a dynamic buffer would silently end up as a regular one.
        const char* pTypeName = binding->block.type_description ? binding->block.type_description->type_name : nullptr;

        if (!pTypeName && (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                           binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
            Log::Warning("Buffer descriptor at set {} binding {} has no type name, so it cannot be identified as "
                         "dynamic. The SPIR-V was built without debug information.",
                         binding->set, binding->binding);
        }

        if (pTypeName && std::string_view(pTypeName).ends_with("Dynamic")) {
            switch (binding->descriptor_type) {
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            }
        }

        return static_cast<VkDescriptorType>(binding->descriptor_type);
    };

    // Create descriptor set layouts
    mDescriptorSetLayouts.clear();
    mDescriptorSetLayouts.resize(descriptorSets.size());

    // Resources are looked up by name, and the pool has to be able to serve every binding the shader declares
    mResourceInfos.clear();
    std::map<VkDescriptorType, uint32_t> poolCounts;

    // Registers a name so that setResource() can find the binding. Blocks declared without an instance name have no
    // variable name, and are reachable through their block type name instead.
    auto registerName = [&](const char* pName, const ResourceInfo& info) {
        if (!pName || !pName[0]) {
            return;
        }
        auto [it, inserted] = mResourceInfos.emplace(pName, info);
        if (!inserted && (it->second.set != info.set || it->second.binding != info.binding)) {
            Log::Warning("Shader declares '{}' at more than one binding, set {} binding {} will not be reachable by "
                         "name. Bind it explicitly by set and binding instead.",
                         pName, info.set, info.binding);
        }
    };

    for (uint32_t i = 0; i < count(descriptorSets); i++) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (auto& [bindingNumber, binding] : descriptorSets[i]) {
            // A count is missing only if the binding was rejected above, in which case an error has been logged already
            auto found = bindingCount[i].find(bindingNumber);
            VkDescriptorType type = getType(binding);
            uint32_t descriptorCount = found != bindingCount[i].end() ? found->second : 1;

            bindings.push_back({
                .binding = bindingNumber,
                .descriptorType = type,
                .descriptorCount = descriptorCount,
                .stageFlags = stageFlags[i],
            });

            ResourceInfo info = {.set = i, .binding = bindingNumber, .type = type, .count = descriptorCount};
            registerName(binding->name, info);
            if (binding->block.type_description) {
                registerName(binding->block.type_description->type_name, info);
            }

            poolCounts[type] += descriptorCount;
        }

        VkDescriptorSetLayoutCreateInfo ci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = count(bindings),
            .pBindings = bindings.data(),
        };

        Check::Vk(vkCreateDescriptorSetLayout(mpDevice->getDevice(), &ci, nullptr, &mDescriptorSetLayouts[i]));
    }

    // Prepare descriptor set storage. A pool holds several generations of every set, so that resources can be
    // reattached a number of times before another pool is needed.
    mDescriptorSets.assign(descriptorSets.size(), VK_NULL_HANDLE);
    mSetDirty.assign(descriptorSets.size(), true);

    const uint32_t generationsPerPool = 8;
    mPoolSizes.clear();
    for (auto& [type, descriptorCount] : poolCounts) {
        mPoolSizes.push_back({.type = type, .descriptorCount = descriptorCount * generationsPerPool});
    }
    mMaxSetsPerPool = count(descriptorSets) * generationsPerPool;

    // Create push constant ranges
    mPushConstantRanges.clear();
    for (size_t i = 0; i < mReflections.size(); i++) {
        if (!mReflections[i]) {
            continue; // Stage failed to load, already reported above
        }

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

const Shader::ResourceInfo* Shader::resolveResource(const std::string& name, const char* kind,
                                                    std::initializer_list<VkDescriptorType> allowed)
{
    auto found = mResourceInfos.find(name);
    if (found == mResourceInfos.end()) {
        Log::Error("Shader has no resource named '{}'. Check the spelling against the shader, and note that a block "
                   "declared without an instance name is reached through its block type name.",
                   name);
        return nullptr;
    }

    const ResourceInfo& info = found->second;
    if (std::find(allowed.begin(), allowed.end(), info.type) == allowed.end()) {
        Log::Error("Resource '{}' is declared as descriptor type {} in the shader and cannot be set from a {}.", name,
                   static_cast<int>(info.type), kind);
        return nullptr;
    }

    return &info;
}

void Shader::setResource(const std::string& name, ptr<Buffer> pBuffer, VkDeviceSize offset, VkDeviceSize range)
{
    const ResourceInfo* pInfo =
        resolveResource(name, "buffer",
                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC});
    if (!pInfo) {
        return;
    }

    BoundResource resource;
    resource.pBuffer = pBuffer;
    resource.offset = offset;
    resource.range = range;

    mResources[name] = resource;
    mSetDirty[pInfo->set] = true;
}

void Shader::setResource(const std::string& name, ptr<Texture> pTexture)
{
    const ResourceInfo* pInfo = resolveResource(name, "texture", {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});
    if (!pInfo) {
        return;
    }

    if (pInfo->count != 1) {
        Log::Error("Resource '{}' is an array of {} descriptors, so it needs an array of textures.", name,
                   pInfo->count);
        return;
    }

    BoundResource resource;
    resource.pTexture = pTexture;

    mResources[name] = resource;
    mSetDirty[pInfo->set] = true;
}

void Shader::setResource(const std::string& name, const std::vector<ptr<Texture>>& textures)
{
    const ResourceInfo* pInfo = resolveResource(name, "texture array", {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});
    if (!pInfo) {
        return;
    }

    if (count(textures) != pInfo->count) {
        Log::Error("Resource '{}' has {} descriptors in the shader but {} textures were given.", name, pInfo->count,
                   count(textures));
        return;
    }

    BoundResource resource;
    resource.textures = textures;

    mResources[name] = resource;
    mSetDirty[pInfo->set] = true;
}

void Shader::setResource(const std::string& name, ptr<Image> pImage, VkImageLayout layout)
{
    const ResourceInfo* pInfo =
        resolveResource(name, "image", {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT});
    if (!pInfo) {
        return;
    }

    BoundResource resource;
    resource.pImage = pImage;
    resource.imageLayout = layout;

    mResources[name] = resource;
    mSetDirty[pInfo->set] = true;
}

void Shader::setResource(const std::string& name, ptr<AccelerationStructure> pAccelerationStructure)
{
    const ResourceInfo* pInfo =
        resolveResource(name, "acceleration structure", {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR});
    if (!pInfo) {
        return;
    }

    BoundResource resource;
    resource.pAccelerationStructure = pAccelerationStructure;

    mResources[name] = resource;
    mSetDirty[pInfo->set] = true;
}

void Shader::createDescriptorPool()
{
    if (mPoolSizes.empty()) {
        return;
    }

    VkDescriptorPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = mMaxSetsPerPool,
        .poolSizeCount = count(mPoolSizes),
        .pPoolSizes = mPoolSizes.data(),
    };

    VkDescriptorPool pool;
    Check::Vk(vkCreateDescriptorPool(mpDevice->getDevice(), &ci, nullptr, &pool));
    mDescriptorPools.push_back(pool);
}

VkDescriptorSet Shader::allocateDescriptorSet(uint32_t set)
{
    if (mDescriptorPools.empty()) {
        createDescriptorPool();
    }

    VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mDescriptorPools.back(),
        .descriptorSetCount = 1,
        .pSetLayouts = &mDescriptorSetLayouts[set],
    };

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult result = vkAllocateDescriptorSets(mpDevice->getDevice(), &ai, &descriptorSet);

    // Older sets are kept alive on purpose, so a pool runs out after enough reattachments and another is added
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        createDescriptorPool();
        ai.descriptorPool = mDescriptorPools.back();
        result = vkAllocateDescriptorSets(mpDevice->getDevice(), &ai, &descriptorSet);
    }

    Check::Vk(result);
    return descriptorSet;
}

void Shader::updateDescriptorSet(uint32_t set)
{
    std::vector<VkWriteDescriptorSet> writes;

    // The writes point into these, and a deque never moves what it already holds
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::deque<std::vector<VkDescriptorImageInfo>> imageInfos;
    std::deque<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructureInfos;
    std::deque<VkAccelerationStructureKHR> accelerationStructures;

    for (const auto& [name, resource] : mResources) {
        auto found = mResourceInfos.find(name);
        if (found == mResourceInfos.end() || found->second.set != set) {
            continue;
        }
        const ResourceInfo& info = found->second;

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = mDescriptorSets[set],
            .dstBinding = info.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = info.type,
        };

        switch (info.type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            bufferInfos.push_back({
                .buffer = resource.pBuffer->getBuffer(),
                .offset = resource.offset,
                .range = resource.range,
            });
            write.pBufferInfo = &bufferInfos.back();
            break;

        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
            imageInfos.emplace_back();
            auto& infos = imageInfos.back();
            if (!resource.textures.empty()) {
                for (const auto& pTexture : resource.textures) {
                    infos.push_back({
                        .sampler = pTexture->getSampler(),
                        .imageView = pTexture->getImageView(),
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    });
                }
            } else {
                infos.push_back({
                    .sampler = resource.pTexture->getSampler(),
                    .imageView = resource.pTexture->getImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                });
            }
            write.descriptorCount = count(infos);
            write.pImageInfo = infos.data();
            break;
        }

        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
            VkImageLayout layout = resource.imageLayout;
            if (layout == VK_IMAGE_LAYOUT_UNDEFINED) {
                layout = info.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ? VK_IMAGE_LAYOUT_GENERAL
                                                                       : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            imageInfos.emplace_back();
            imageInfos.back().push_back({
                .sampler = VK_NULL_HANDLE,
                .imageView = resource.pImage->getImageView(),
                .imageLayout = layout,
            });
            write.pImageInfo = imageInfos.back().data();
            break;
        }

        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
            accelerationStructures.push_back(resource.pAccelerationStructure->getAccelerationStructure());
            accelerationStructureInfos.push_back({
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                .accelerationStructureCount = 1,
                .pAccelerationStructures = &accelerationStructures.back(),
            });
            write.pNext = &accelerationStructureInfos.back();
            break;

        default:
            Log::Error("Resource '{}' has a descriptor type that cannot be written.", name);
            continue;
        }

        writes.push_back(write);
    }

    if (!writes.empty()) {
        vkUpdateDescriptorSets(mpDevice->getDevice(), count(writes), writes.data(), 0, nullptr);
    }
}

void Shader::bindResources(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, uint32_t set,
                           const std::vector<uint32_t>& dynamicOffsets)
{
    if (set >= mDescriptorSets.size()) {
        Log::Error("Trying to bind set {}, but the shader only declares {} sets.", set, mDescriptorSets.size());
        return;
    }

    if (mSetDirty[set]) {
        // A set that is still being read by a frame in flight is left alone, a new one is taken instead
        mDescriptorSets[set] = allocateDescriptorSet(set);
        updateDescriptorSet(set);
        mSetDirty[set] = false;
    }

    if (mDescriptorSets[set] == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindDescriptorSets(cmd, bindPoint, mPipelineLayout, set, 1, &mDescriptorSets[set], count(dynamicOffsets),
                            dynamicOffsets.data());
}

void Shader::bindResources(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint)
{
    // Only sets that actually have something attached are bound, so that a shader can be used before every set has
    // been filled in
    std::set<uint32_t> setsWithResources;
    for (const auto& [name, resource] : mResources) {
        auto found = mResourceInfos.find(name);
        if (found != mResourceInfos.end()) {
            setsWithResources.insert(found->second.set);
        }
    }

    for (uint32_t set : setsWithResources) {
        bindResources(cmd, bindPoint, set);
    }
}
