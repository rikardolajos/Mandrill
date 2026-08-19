#include "MLP.h"

#include "Error.h"
#include "Helpers.h"
#include "Log.h"

#include <glm/gtc/packing.hpp>

using namespace Mandrill;

// Cooperative vector instructions are issued on half precision values, both for the matrices and for the vectors
static constexpr VkComponentTypeKHR kComponentType = VK_COMPONENT_TYPE_FLOAT16_KHR;

// Alignment the matrices and the bias vectors need inside the parameter buffer
static constexpr size_t kMatrixAlignment = 64;
static constexpr size_t kVectorAlignment = 16;

MLP::MLP(ptr<Device> pDevice, const std::filesystem::path& path) : mpDevice(pDevice), mPath(path)
{
    setupCooperativeVector();

    if (!mpfnVkConvertCooperativeVectorMatrixNV) {
        return;
    }

    std::vector<Layer> layers;
    if (!readFile(path, layers)) {
        return;
    }

    createBuffers(layers);

    mInputCount = layers.front().inputs;
    mOutputCount = layers.back().outputs;
    mLayerCount = count(layers);
    mHiddenWidth = layers.front().outputs;

    setupSpecializationConstants();

    Log::Info("Loaded MLP from {}: {} inputs, {} outputs, {} layers of width {}", path.string(), mInputCount,
              mOutputCount, mLayerCount, mHiddenWidth);
}

MLP::~MLP()
{
}

bool MLP::readFile(const std::filesystem::path& path, std::vector<Layer>& layers)
{
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open()) {
        Log::Error("Unable to open {}", path.string());
        return false;
    }

    // Every read is checked at the end through the stream state, so a truncated file is caught even if the layer
    // counts it claims look sensible
    auto readUint = [&is]() {
        uint32_t value = 0;
        is.read(reinterpret_cast<char*>(&value), sizeof(uint32_t));
        return value;
    };

    uint32_t version = readUint();
    if (version != kFileVersion) {
        Log::Error("{} is an MLP of version {}, expected version {}", path.string(), version, kFileVersion);
        return false;
    }

    uint32_t layerCount = readUint();

    if (layerCount == 0) {
        Log::Error("{} holds no layers", path.string());
        return false;
    }

    layers.resize(layerCount);

    for (uint32_t i = 0; i < layerCount; i++) {
        Layer& layer = layers[i];

        // PyTorch stores a Linear weight as [out_features, in_features]
        layer.outputs = readUint();
        layer.inputs = readUint();

        layer.weights.resize(static_cast<size_t>(layer.outputs) * layer.inputs);
        is.read(reinterpret_cast<char*>(layer.weights.data()), layer.weights.size() * sizeof(float));

        uint32_t biasCount = readUint();
        layer.biases.resize(biasCount);
        is.read(reinterpret_cast<char*>(layer.biases.data()), layer.biases.size() * sizeof(float));

        if (!is) {
            Log::Error("{} ended while reading layer {}", path.string(), i);
            return false;
        }

        if (biasCount != layer.outputs) {
            Log::Error("Layer {} of {} has {} outputs but {} biases", i, path.string(), layer.outputs, biasCount);
            return false;
        }
    }

    warnUnlessPlainStack(path, layers);

    return true;
}

void MLP::warnUnlessPlainStack(const std::filesystem::path& path, const std::vector<Layer>& layers) const
{
    // Nothing in this class depends on how the layers are wired together, it packs and converts them the same way
    // whatever the topology is. The mlpForward() that comes with the framework does depend on it: it runs the hidden
    // layers in a loop, on a cooperative vector of one fixed width, so a network shaped differently is loaded but
    // needs a forward pass written for it.
    static constexpr const char* kNote =
        "the mlpForward() in MLP.glsl evaluates a plain stack of equally wide hidden layers, so this network needs a "
        "forward pass of its own";

    if (layers.size() < 2) {
        Log::Warning("{} has a single layer, {}", path.string(), kNote);
        return;
    }

    for (uint32_t i = 1; i < count(layers); i++) {
        if (layers[i].inputs != layers[i - 1].outputs) {
            Log::Warning("Layer {} of {} takes {} inputs while the previous layer produces {} outputs, {}", i,
                         path.string(), layers[i].inputs, layers[i - 1].outputs, kNote);
            return;
        }
    }

    uint32_t hiddenWidth = layers.front().outputs;
    for (uint32_t i = 1; i < count(layers) - 1; i++) {
        if (layers[i].outputs != hiddenWidth) {
            Log::Warning("Hidden layer {} of {} is {} neurons wide while the first layer is {}, {}", i, path.string(),
                         layers[i].outputs, hiddenWidth, kNote);
            return;
        }
    }
}

void MLP::createBuffers(const std::vector<Layer>& layers)
{
    uint32_t layerCount = count(layers);

    std::vector<size_t> weightSizes(layerCount);
    std::vector<uint32_t> weightOffsets(layerCount);
    std::vector<size_t> biasSizes(layerCount);
    std::vector<uint32_t> biasOffsets(layerCount);

    // Lay the layers out back to back in one buffer, the shader reaches each of them through its offset
    size_t offset = 0;
    for (uint32_t i = 0; i < layerCount; i++) {
        weightSizes[i] = coopVecQuerySize(layers[i].outputs, layers[i].inputs);
        biasSizes[i] = layers[i].biases.size() * sizeof(uint16_t);

        offset = Helpers::alignTo(offset, kMatrixAlignment);
        weightOffsets[i] = static_cast<uint32_t>(offset);
        offset += weightSizes[i];

        offset = Helpers::alignTo(offset, kVectorAlignment);
        biasOffsets[i] = static_cast<uint32_t>(offset);
        offset += biasSizes[i];
    }

    std::vector<uint8_t> params(offset, 0);

    for (uint32_t i = 0; i < layerCount; i++) {
        // The driver rearranges the matrix into whichever layout the hardware reads fastest, the shader only has to
        // tell it that the matrix is in that layout
        coopVecConvert(params.data() + weightOffsets[i], weightSizes[i], layers[i].weights.data(), layers[i].outputs,
                       layers[i].inputs);

        // Biases are a plain half precision vector, they are converted here
        std::vector<uint16_t> biases(layers[i].biases.size());
        std::transform(layers[i].biases.begin(), layers[i].biases.end(), biases.begin(),
                       [](float v) { return glm::packHalf1x16(v); });
        std::memcpy(params.data() + biasOffsets[i], biases.data(), biasSizes[i]);
    }

    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    mpParams = mpDevice->createBuffer(params.size(), usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpParams->copyFromHost(params.data(), params.size());

    mpWeightOffsets =
        mpDevice->createBuffer(weightOffsets.size() * sizeof(uint32_t), usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpWeightOffsets->copyFromHost(weightOffsets.data(), weightOffsets.size() * sizeof(uint32_t));

    mpBiasOffsets =
        mpDevice->createBuffer(biasOffsets.size() * sizeof(uint32_t), usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpBiasOffsets->copyFromHost(biasOffsets.data(), biasOffsets.size() * sizeof(uint32_t));

    // The three buffers above are reached by address, so the only thing the shader binds is where they are
    mpLocations = mpDevice->createBuffer(sizeof(MLPLocations),
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    MLPLocations locations = {
        .params = mpParams->getDeviceAddress(),
        .weightOffsets = mpWeightOffsets->getDeviceAddress(),
        .biasOffsets = mpBiasOffsets->getDeviceAddress(),
    };

    mpLocations->copyFromHost(&locations, sizeof(MLPLocations));
}

void MLP::setupSpecializationConstants()
{
    mSpecializationConstants = {mInputCount, mOutputCount, mLayerCount, mHiddenWidth};

    for (uint32_t i = 0; i < count(mSpecializationMapEntries); i++) {
        mSpecializationMapEntries[i] = {
            .constantID = i,
            .offset = i * static_cast<uint32_t>(sizeof(uint32_t)),
            .size = sizeof(uint32_t),
        };
    }

    mSpecializationInfo = {
        .mapEntryCount = count(mSpecializationMapEntries),
        .pMapEntries = mSpecializationMapEntries.data(),
        .dataSize = mSpecializationConstants.size() * sizeof(uint32_t),
        .pData = mSpecializationConstants.data(),
    };
}

void MLP::appendSpecializationConstants(std::vector<VkSpecializationMapEntry>& mapEntries,
                                        std::vector<uint32_t>& values, uint32_t firstConstantId) const
{
    if (!isValid()) {
        Log::Error("MLP::appendSpecializationConstants() - No network has been loaded");
        return;
    }

    for (uint32_t i = 0; i < kSpecializationConstantCount; i++) {
        // The offset has to be taken before the value is appended, it is where this value is about to land
        mapEntries.push_back({
            .constantID = firstConstantId + i,
            .offset = count(values) * static_cast<uint32_t>(sizeof(uint32_t)),
            .size = sizeof(uint32_t),
        });

        values.push_back(mSpecializationConstants[i]);
    }
}

size_t MLP::coopVecQuerySize(uint32_t rows, uint32_t columns) const
{
    size_t dstSize = 0;

    VkConvertCooperativeVectorMatrixInfoNV info = {
        .sType = VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV,
        .srcSize = sizeof(float) * rows * columns,
        .pDstSize = &dstSize,
        .srcComponentType = VK_COMPONENT_TYPE_FLOAT32_KHR,
        .dstComponentType = kComponentType,
        .numRows = rows,
        .numColumns = columns,
        .srcLayout = VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV,
        .srcStride = sizeof(float) * columns,
        .dstLayout = VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV,
    };

    Check::Vk(mpfnVkConvertCooperativeVectorMatrixNV(mpDevice->getDevice(), &info));

    return dstSize;
}

void MLP::coopVecConvert(void* pDst, size_t dstSize, const void* pSrc, uint32_t rows, uint32_t columns) const
{
    VkConvertCooperativeVectorMatrixInfoNV info = {
        .sType = VK_STRUCTURE_TYPE_CONVERT_COOPERATIVE_VECTOR_MATRIX_INFO_NV,
        .srcSize = sizeof(float) * rows * columns,
        .srcData = {.hostAddress = pSrc},
        .pDstSize = &dstSize,
        .dstData = {.hostAddress = pDst},
        .srcComponentType = VK_COMPONENT_TYPE_FLOAT32_KHR,
        .dstComponentType = kComponentType,
        .numRows = rows,
        .numColumns = columns,
        .srcLayout = VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_ROW_MAJOR_NV,
        .srcStride = sizeof(float) * columns,
        .dstLayout = VK_COOPERATIVE_VECTOR_MATRIX_LAYOUT_INFERENCING_OPTIMAL_NV,
        .dstStride = 0,
    };

    Check::Vk(mpfnVkConvertCooperativeVectorMatrixNV(mpDevice->getDevice(), &info));
}

void MLP::setupCooperativeVector(bool printDebug)
{
    mpfnVkConvertCooperativeVectorMatrixNV = reinterpret_cast<PFN_vkConvertCooperativeVectorMatrixNV>(
        vkGetInstanceProcAddr(mpDevice->getInstance(), "vkConvertCooperativeVectorMatrixNV"));

    if (!mpfnVkConvertCooperativeVectorMatrixNV) {
        Log::Error("Cooperative vector is not supported, the device has to be created with {} and the "
                   "cooperativeVector feature",
                   VK_NV_COOPERATIVE_VECTOR_EXTENSION_NAME);
        return;
    }

    if (!printDebug) {
        return;
    }

    auto coopVecPropsFunc = reinterpret_cast<PFN_vkGetPhysicalDeviceCooperativeVectorPropertiesNV>(
        vkGetInstanceProcAddr(mpDevice->getInstance(), "vkGetPhysicalDeviceCooperativeVectorPropertiesNV"));

    if (!coopVecPropsFunc) {
        return;
    }

    std::map<VkComponentTypeKHR, std::string> types = {
        {VK_COMPONENT_TYPE_FLOAT16_KHR, "VK_COMPONENT_TYPE_FLOAT16_KHR"},
        {VK_COMPONENT_TYPE_FLOAT32_KHR, "VK_COMPONENT_TYPE_FLOAT32_KHR"},
        {VK_COMPONENT_TYPE_FLOAT64_KHR, "VK_COMPONENT_TYPE_FLOAT64_KHR"},
        {VK_COMPONENT_TYPE_SINT8_KHR, "VK_COMPONENT_TYPE_SINT8_KHR"},
        {VK_COMPONENT_TYPE_SINT16_KHR, "VK_COMPONENT_TYPE_SINT16_KHR"},
        {VK_COMPONENT_TYPE_SINT32_KHR, "VK_COMPONENT_TYPE_SINT32_KHR"},
        {VK_COMPONENT_TYPE_SINT64_KHR, "VK_COMPONENT_TYPE_SINT64_KHR"},
        {VK_COMPONENT_TYPE_UINT8_KHR, "VK_COMPONENT_TYPE_UINT8_KHR"},
        {VK_COMPONENT_TYPE_UINT16_KHR, "VK_COMPONENT_TYPE_UINT16_KHR"},
        {VK_COMPONENT_TYPE_UINT32_KHR, "VK_COMPONENT_TYPE_UINT32_KHR"},
        {VK_COMPONENT_TYPE_UINT64_KHR, "VK_COMPONENT_TYPE_UINT64_KHR"},
        {VK_COMPONENT_TYPE_SINT8_PACKED_NV, "VK_COMPONENT_TYPE_SINT8_PACKED_NV"},
        {VK_COMPONENT_TYPE_UINT8_PACKED_NV, "VK_COMPONENT_TYPE_UINT8_PACKED_NV"},
        {VK_COMPONENT_TYPE_FLOAT_E4M3_NV, "VK_COMPONENT_TYPE_FLOAT_E4M3_NV"},
        {VK_COMPONENT_TYPE_FLOAT_E5M2_NV, "VK_COMPONENT_TYPE_FLOAT_E5M2_NV"},
    };

    Log::Debug("Cooperative vector supported with following types");
    uint32_t n;
    Check::Vk(coopVecPropsFunc(mpDevice->getPhysicalDevice(), &n, nullptr));
    std::vector<VkCooperativeVectorPropertiesNV> props(n);
    std::for_each(props.begin(), props.end(),
                  [](auto& prop) { prop.sType = VK_STRUCTURE_TYPE_COOPERATIVE_VECTOR_PROPERTIES_NV; });
    Check::Vk(coopVecPropsFunc(mpDevice->getPhysicalDevice(), &n, props.data()));
    for (uint32_t i = 0; i < count(props); i++) {
        auto& p = props[i];
        Log::Debug("Set {}", i);
        Log::Debug("\tinput type:            {}\n"
                   "\tinput interpretation:  {}\n"
                   "\tmatrix interpretation: {}\n"
                   "\tbias interpretation:   {}\n"
                   "\tresult type:           {}\n"
                   "\ttranspose:             {}",
                   types[p.inputType], types[p.inputInterpretation], types[p.matrixInterpretation],
                   types[p.biasInterpretation], types[p.resultType], p.transpose ? "true" : "false");
    }
}

void MLP::attachTo(ptr<Shader> pShader, const std::string& name)
{
    if (!isValid()) {
        Log::Error("MLP::attachTo() - No network has been loaded");
        return;
    }

    mpShader = pShader;
    mResourceName = name;

    pShader->setResource(name, mpLocations);
}

void MLP::bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint)
{
    if (!mpShader) {
        Log::Error("MLP::bind() - The MLP has not been attached to a shader");
        return;
    }

    auto info = mpShader->getResourceInfo(mResourceName);
    if (info) {
        mpShader->bindResources(cmd, bindPoint, info->set);
    }
}
