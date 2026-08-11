#include "EnvironmentMap.h"

#include "Log.h"

#include <stb_image.h>

#include <cmath>

using namespace Mandrill;

namespace
{
    constexpr float PI = 3.14159265358979323846f;

    // Keeps the density non-zero everywhere, which bounds radiance / pdf in regions where the map is black
    constexpr float DENSITY_FLOOR = 1e-4f;
} // namespace

EnvironmentMap::EnvironmentMap(ptr<Device> pDevice, const std::filesystem::path& path, VkFormat format)
    : mpDevice(pDevice)
{
    mpTexture = make_ptr<Texture>(pDevice, TextureType::Texture2D, format, path, false);

    std::filesystem::path fullPath = path;
    if (path.is_relative()) {
        fullPath = GetExecutablePath() / path;
    }

    // The distribution has to match the orientation of the texture, which Texture loads flipped
    stbi_set_flip_vertically_on_load(1);

    std::string pathStr = fullPath.string();
    int width, height, channels;
    float* pData = stbi_loadf(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pData) {
        Log::Error("Failed to load environment map for importance sampling");
        return;
    }

    createDistribution(pData, width, height);

    stbi_image_free(pData);
}

EnvironmentMap::EnvironmentMap(ptr<Device> pDevice) : mpDevice(pDevice)
{
    const float whiteTexel[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    mpTexture = make_ptr<Texture>(pDevice, TextureType::Texture2D, VK_FORMAT_R32G32B32A32_SFLOAT, whiteTexel, 1, 1, 1,
                                  static_cast<uint32_t>(sizeof whiteTexel), false);

    createDistribution(whiteTexel, 1, 1);
}

EnvironmentMap::~EnvironmentMap()
{
}

void EnvironmentMap::createDistribution(const float* pData, uint32_t width, uint32_t height)
{
    mWidth = width;
    mHeight = height;

    std::vector<float> conditional(static_cast<size_t>(height) * (static_cast<size_t>(width) + 1));
    std::vector<float> marginal(static_cast<size_t>(height) + 1);
    std::vector<float> rowIntegral(height);

    for (uint32_t j = 0; j < height; j++) {
        // Rows near the poles cover less solid angle, so they should be sampled less often
        const float sinTheta = std::sin(PI * (static_cast<float>(j) + 0.5f) / static_cast<float>(height));

        float* pRow = &conditional[static_cast<size_t>(j) * (static_cast<size_t>(width) + 1)];
        pRow[0] = 0.0f;
        for (uint32_t i = 0; i < width; i++) {
            const float* pTexel = &pData[4 * (static_cast<size_t>(j) * width + i)];
            const float luminance = 0.2126f * pTexel[0] + 0.7152f * pTexel[1] + 0.0722f * pTexel[2];
            pRow[i + 1] = pRow[i] + (luminance + DENSITY_FLOOR) * sinTheta / static_cast<float>(width);
        }

        // The density floor guarantees a positive integral, so no division by zero is possible here
        rowIntegral[j] = pRow[width];
        for (uint32_t i = 1; i <= width; i++) {
            pRow[i] /= rowIntegral[j];
        }
    }

    marginal[0] = 0.0f;
    for (uint32_t j = 0; j < height; j++) {
        marginal[j + 1] = marginal[j] + rowIntegral[j] / static_cast<float>(height);
    }

    const float total = marginal[height];
    for (uint32_t j = 1; j <= height; j++) {
        marginal[j] /= total;
    }

    const VkDeviceSize marginalSize = marginal.size() * sizeof(float);
    const VkDeviceSize conditionalSize = conditional.size() * sizeof(float);

    mpMarginal = make_ptr<Buffer>(mpDevice, marginalSize,
                                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpMarginal->copyFromHost(marginal.data(), marginalSize);

    mpConditional = make_ptr<Buffer>(mpDevice, conditionalSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpConditional->copyFromHost(conditional.data(), conditionalSize);
}
