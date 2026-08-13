#include "Mandrill.h"

using namespace Mandrill;

class VolumeViewer : public App
{
public:
    // Feature flags, must match RayMarcher.frag
    enum RayMarcherFlags : uint32_t {
        FLAG_PATH_TRACING = 1 << 0,
        FLAG_ACCUMULATE = 1 << 1,
        FLAG_MULTI_SCATTER = 1 << 2,
        FLAG_NEE = 1 << 3,
        FLAG_RUSSIAN_ROULETTE = 1 << 4,
        FLAG_ENV_LIGHT = 1 << 5,
        FLAG_HG_PHASE = 1 << 6,
        FLAG_TONEMAP = 1 << 7,
        FLAG_ENV_IMPORTANCE = 1 << 8,
        FLAG_MIS = 1 << 9,
        FLAG_DEBUG_TRUNCATION = 1 << 10,
    };

    // Value of the NEE depth field that means "no limit", must match RayMarcher.frag
    static constexpr int NEE_DEPTH_UNLIMITED = 255;

    // Push constants are limited to 128 bytes on some devices, hence the packing of bounces and samples
    struct PushConstant {
        glm::mat4 inverseModel;
        glm::vec3 gridMin;
        float phaseG;
        glm::vec3 gridMax;
        float envIntensity;
        glm::vec2 viewport;
        uint32_t frameIndex;
        uint32_t flags;
        uint32_t bouncesAndSamples;
        float albedo;
        uint32_t seed;
        float exposure;
    };
    static_assert(sizeof(PushConstant) == 128, "PushConstant must match the block in RayMarcher.frag");

    struct SpecializationConstants {
        int maxSteps;
        float stepSize;
        float density;
    };

    VolumeViewer() : App("VolumeViewer", 1920, 1080)
    {
        // Create a Vulkan instance and device
        std::vector<const char*> extensions = {
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        };
        mpDevice = std::make_shared<Device>(mpWindow, extensions);

        // Create a swapchain with 2 frames in flight (default)
        mpSwapchain = mpDevice->createSwapchain();

        // Create a pass with 1 color attachment, depth attachment and multisampling
        mpPass = mpDevice->createPass(mpSwapchain->getExtent(), mpSwapchain->getImageFormat(), 1, true,
                                      mpDevice->getSampleCount());

        // Prepare vertex binding and attribute descriptions with empty vectors (only fullscreen triangles are used)
        std::vector<VkVertexInputBindingDescription> emptyBindingDescription;
        std::vector<VkVertexInputAttributeDescription> emptyAttributeDescription;
        PipelineDesc pipelineDesc = PipelineDesc(emptyBindingDescription, emptyAttributeDescription);
        pipelineDesc.depthTestEnable = VK_FALSE;

        // Create a pipeline for environment map
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("VolumeViewer/Fullscreen.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("VolumeViewer/Environment.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pEnvMapShader = mpDevice->createShader(shaderDesc);
        mpEnvironmentMapPipeline = mpDevice->createPipeline(mpPass, pEnvMapShader, pipelineDesc);

        // Specialization constants for ray marching shader
        mSpecializationConstants = {.maxSteps = 1000, .stepSize = 0.01f, .density = 1.0f};
        mSpecializationMapEntries.push_back(
            {.constantID = 0, .offset = offsetof(SpecializationConstants, maxSteps), .size = sizeof(uint32_t)});
        mSpecializationMapEntries.push_back(
            {.constantID = 1, .offset = offsetof(SpecializationConstants, stepSize), .size = sizeof(float)});
        mSpecializationMapEntries.push_back(
            {.constantID = 2, .offset = offsetof(SpecializationConstants, density), .size = sizeof(float)});
        mSpecializationInfo = {
            .mapEntryCount = count(mSpecializationMapEntries),
            .pMapEntries = mSpecializationMapEntries.data(),
            .dataSize = sizeof mSpecializationConstants,
            .pData = &mSpecializationConstants,
        };

        // Create a pipeline for ray marching
        shaderDesc.clear();
        shaderDesc.emplace_back("VolumeViewer/Fullscreen.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("VolumeViewer/RayMarcher.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT,
                                &mSpecializationInfo);
        auto pRayMarchShader = mpDevice->createShader(shaderDesc);

        pipelineDesc.depthTestEnable = VK_TRUE;
        pipelineDesc.colorBlendAttachmentStates[0].blendEnable = VK_TRUE;
        mpRayMarchingPipeline = mpDevice->createPipeline(mpPass, pRayMarchShader, pipelineDesc);

        mPipelines = {mpEnvironmentMapPipeline, mpRayMarchingPipeline};

        // Setup camera
        mpCamera = mpDevice->createCamera(mpSwapchain->getFramesInFlightCount());
        mpCamera->setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Attach the camera uniforms to both shaders. The buffer holds one copy per frame in flight and is bound
        // with a dynamic offset, so the range covers a single copy.
        mpEnvironmentMapPipeline->getShader()->setResource("camera", mpCamera->getUniformBuffer(), 0,
                                                           sizeof(CameraMatrices));
        mpRayMarchingPipeline->getShader()->setResource("camera", mpCamera->getUniformBuffer(), 0,
                                                        sizeof(CameraMatrices));

        // Environment maps are loaded into a float format to keep the dynamic range of HDR files. Prefer full float,
        // but fall back to half float if the device cannot filter it.
        mEnvironmentMapFormat = Helpers::findSupportedFormat(
            mpDevice, {VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT}, VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

        // Fallback environment map (plain white) used by the ray marcher until one is loaded
        mpDummyEnvironmentMap = std::make_shared<EnvironmentMap>(mpDevice);
        setEnvironmentMapResources();

        // Buffer for temporal accumulation
        createAccumulationImage();

        // Initialize GUI
        App::createGUI(mpDevice, mpPass);
    }

    ~VolumeViewer()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta)
    {
        mpSwapchain->waitForFence();

        if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
            mpCamera->update(mpWindow, delta, getCursorDelta(), mpSwapchain->getInFlightIndex());
        } else {
            // Still refresh the uniforms, otherwise a resize while the GUI holds the input focus would never
            // reach the shader with the new projection matrix
            mpCamera->update(mpSwapchain->getInFlightIndex());
        }
    }

    void render() override
    {
        // Check if camera matrix and attachments need to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->setAspectRatio(mpSwapchain->getAspectRatio());
            // update() ran before the swapchain was recreated, so push the new projection for this frame too
            mpCamera->update(mpSwapchain->getInFlightIndex());
            mpPass->update(mpSwapchain->getExtent());
            // The accumulation buffer must match the new resolution
            createAccumulationImage();
            resetAccumulation();
        }

        // Restart accumulation whenever the camera moves
        glm::mat4 view = mpCamera->getViewMatrix(mpSwapchain->getInFlightIndex());
        if (view != mPrevView) {
            mPrevView = view;
            resetAccumulation();
        }

        // Acquire frame from swapchain and prepare rasterizer
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();

        // Make last frame's accumulation writes visible before this frame reads them
        Helpers::imageBarrier(cmd, mpAccumImage->getImage(), VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                              VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);

        mpPass->begin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        // Offset that picks this frame's copy of the camera matrices out of the shared uniform buffer
        VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
        uint32_t cameraDescriptorOffset = static_cast<uint32_t>(Helpers::alignTo(sizeof(CameraMatrices), alignment) *
                                                                mpSwapchain->getInFlightIndex());

        // Push constants. The viewport must be the current framebuffer size, since the shaders turn fragment
        // coordinates into ray directions with it. App::mWidth and mHeight are the initial window size and do not
        // follow resizing.
        VkExtent2D extent = mpSwapchain->getExtent();

        glm::vec3 volumeDim(1.0f);
        if (mpVolume) {
            volumeDim = glm::vec3(mpVolume->getImage()->getWidth(), mpVolume->getImage()->getHeight(),
                                  mpVolume->getImage()->getDepth());
        }
        glm::vec3 gridMin = mVolumeModelPosition - (mVolumeModelScale * volumeDim / 2.0f);
        glm::vec3 gridMax = gridMin + mVolumeModelScale * volumeDim;

        uint32_t flags = 0;
        flags |= mPathTracing ? FLAG_PATH_TRACING : 0;
        flags |= mAccumulate ? FLAG_ACCUMULATE : 0;
        flags |= mMultiScatter ? FLAG_MULTI_SCATTER : 0;
        flags |= mNextEventEstimation ? FLAG_NEE : 0;
        flags |= mRussianRoulette ? FLAG_RUSSIAN_ROULETTE : 0;
        flags |= mEnvironmentLight ? FLAG_ENV_LIGHT : 0;
        flags |= mHenyeyGreenstein ? FLAG_HG_PHASE : 0;
        flags |= mTonemap ? FLAG_TONEMAP : 0;
        flags |= mEnvImportanceSampling ? FLAG_ENV_IMPORTANCE : 0;
        flags |= mMultipleImportanceSampling ? FLAG_MIS : 0;
        flags |= mDebugTruncation ? FLAG_DEBUG_TRUNCATION : 0;

        PushConstant pushConstant = {
            .inverseModel = glm::inverse(mVolumeModelMatrix),
            .gridMin = gridMin,
            .phaseG = mPhaseG,
            .gridMax = gridMax,
            .envIntensity = mEnvIntensity,
            .viewport = glm::vec2(extent.width, extent.height),
            .frameIndex = mAccumulatedFrames,
            .flags = flags,
            .bouncesAndSamples = static_cast<uint32_t>(mMaxBounces) |
                                 (static_cast<uint32_t>(mSamplesPerFrame) << 16) |
                                 (static_cast<uint32_t>(mNeeDepth) << 24),
            .albedo = mAlbedo,
            .seed = mFrameCounter++,
            .exposure = mExposure,
        };
        vkCmdPushConstants(cmd, mpEnvironmentMapPipeline->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof pushConstant, &pushConstant);

        // Render environment map. Set 0 is per-frame and takes the dynamic offset, set 1 holds the map itself.
        if (mpEnvironmentMap) {
            auto pShader = mpEnvironmentMapPipeline->getShader();
            mpEnvironmentMapPipeline->bind(cmd);
            pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, {cameraDescriptorOffset});
            pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 1);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }

        // Render volume
        if (mpVolume) {
            auto pShader = mpRayMarchingPipeline->getShader();
            mpRayMarchingPipeline->bind(cmd);
            pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, {cameraDescriptorOffset});
            pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 1);
            vkCmdDraw(cmd, 3, 1, 0, 0);

            if (mPathTracing && mAccumulate) {
                mAccumulatedFrames++;
            }
        }

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpPass->end(cmd);
        mpSwapchain->present(cmd, mpPass->getOutput());
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        App::baseGUI(mpDevice, mpSwapchain, mPipelines);

        if (ImGui::Begin("Volume Viewer")) {
            bool resetAccum = false;

            ImGui::SeparatorText("Volume");
            if (ImGui::Button("Load##Volume")) {
                mVolumePath = OpenFile(mpWindow, "OpenVDB file (*.vdb)\0*.VDB\0All (*.*)\0*.*\0");
                if (!mVolumePath.empty()) {
                    mpVolume = std::make_shared<Texture>(mpDevice, TextureType::Texture3D, VK_FORMAT_R32_SFLOAT,
                                                         mVolumePath, false);
                    mpVolume->setAddressMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                             VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                             VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

                    mpRayMarchingPipeline->getShader()->setResource("volume", mpVolume);

                    glm::vec3 volumeDim = glm::vec3(mpVolume->getImage()->getWidth(), mpVolume->getImage()->getHeight(),
                                                    mpVolume->getImage()->getDepth());
                    mVolumeModelScale = 1.0f / glm::max(volumeDim.x, glm::max(volumeDim.y, volumeDim.z));
                    resetAccum = true;
                }
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(mVolumePath.string().c_str());
            bool recreatePipeline = false;
            recreatePipeline |= ImGui::DragFloat("Density", &mSpecializationConstants.density, 0.1f, 0.0f, 10000.0f);

            bool newModelMatrix = false;
            newModelMatrix |= ImGui::DragFloat("Scale", &mVolumeModelScale, 0.01f);
            newModelMatrix |= ImGui::DragFloat3("Position", &mVolumeModelPosition.x, 0.01f);
            if (newModelMatrix) {
                mVolumeModelMatrix = glm::scale(glm::vec3(mVolumeModelScale));
                mVolumeModelMatrix = glm::translate(mVolumeModelMatrix, mVolumeModelPosition);
                resetAccum = true;
            }

            ImGui::SeparatorText("Environment Map");
            if (ImGui::Button("Load##EnvMap")) {
                mEnvironmentMapPath =
                    OpenFile(mpWindow, "Supported image files (*.hdr, *.png)\0*.HDR;*.PNG\0All (*.*)\0*.*\0");
                if (!mEnvironmentMapPath.empty()) {
                    mpEnvironmentMap =
                        std::make_shared<EnvironmentMap>(mpDevice, mEnvironmentMapPath, mEnvironmentMapFormat);

                    setEnvironmentMapResources();
                    resetAccum = true;
                }
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(mEnvironmentMapPath.string().c_str());

            ImGui::SeparatorText("Ray Marcher");
            recreatePipeline |= ImGui::DragInt("Max steps", &mSpecializationConstants.maxSteps, 1.0f);

            recreatePipeline |=
                ImGui::DragFloat("Step size", &mSpecializationConstants.stepSize, 0.0001f, 0.0f, 0.0f, "%.4f");

            if (recreatePipeline) {
                mpRayMarchingPipeline->recreate();
                resetAccum = true;
            }

            ImGui::SeparatorText("Path Tracer");
            resetAccum |= ImGui::Checkbox("Path tracing", &mPathTracing);
            if (mPathTracing) {
                resetAccum |= ImGui::Checkbox("Accumulate", &mAccumulate);
                resetAccum |= ImGui::Checkbox("Multi-scatter", &mMultiScatter);
                if (mMultiScatter) {
                    resetAccum |= ImGui::SliderInt("Max bounces", &mMaxBounces, 1, 4096, "%d",
                                                   ImGuiSliderFlags_Logarithmic);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("A dense cloud needs hundreds of scattering events before light escapes. "
                                          "Paths that hit this limit are dropped with whatever energy they still "
                                          "carry, which darkens the volume. Use the truncated paths view to see "
                                          "whether the limit is actually being reached.");
                    }
                }
                resetAccum |= ImGui::Checkbox("Next event estimation", &mNextEventEstimation);
                if (mNextEventEstimation) {
                    resetAccum |= ImGui::Checkbox("Environment importance sampling", &mEnvImportanceSampling);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Sample light directions proportionally to the radiance in the environment "
                                          "map. Without it, a small bright region such as a sun is found only rarely "
                                          "and each hit carries a large weight, which shows up as fireflies.");
                    }
                    resetAccum |= ImGui::Checkbox("Multiple importance sampling", &mMultipleImportanceSampling);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Combine light sampling with phase function sampling, weighting each by how "
                                          "likely it was to produce the direction. Light sampling wins for small "
                                          "bright lights, phase sampling for broad skies and strong anisotropy.");
                    }
                    resetAccum |= ImGui::SliderInt("NEE depth", &mNeeDepth, 1, NEE_DEPTH_UNLIMITED,
                                                   mNeeDepth >= NEE_DEPTH_UNLIMITED ? "unlimited" : "%d",
                                                   ImGuiSliderFlags_Logarithmic);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Stop tracing shadow rays past this scattering event. Deep inside a dense "
                                          "volume they are blocked anyway, so skipping them makes deep paths far "
                                          "cheaper. Past the limit the escaping rays account for the environment on "
                                          "their own, so the result stays unbiased.");
                    }
                }
                resetAccum |= ImGui::Checkbox("Russian roulette", &mRussianRoulette);
                resetAccum |= ImGui::Checkbox("Environment light", &mEnvironmentLight);
                resetAccum |= ImGui::Checkbox("Henyey-Greenstein phase", &mHenyeyGreenstein);
                if (mHenyeyGreenstein) {
                    resetAccum |= ImGui::SliderFloat("Anisotropy", &mPhaseG, -0.99f, 0.99f);
                }
                resetAccum |= ImGui::SliderFloat("Scattering albedo", &mAlbedo, 0.0f, 1.0f, "%.4f");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Fraction of light surviving each scattering event. Clouds are near 1.0; every "
                                      "bit below that compounds over hundreds of events and turns them into smoke.");
                }
                resetAccum |= ImGui::SliderFloat("Environment intensity", &mEnvIntensity, 0.0f, 10.0f);
                resetAccum |= ImGui::SliderInt("Samples per frame", &mSamplesPerFrame, 1, 16);
                ImGui::Text("Accumulated frames: %u", mAccumulatedFrames);
                ImGui::SameLine();
                if (ImGui::Button("Reset")) {
                    resetAccum = true;
                }
            }

            // Display settings do not affect the accumulated radiance, so they must not reset it
            ImGui::SeparatorText("Display");
            ImGui::Checkbox("Tonemap", &mTonemap);
            ImGui::DragFloat("Exposure", &mExposure, 0.01f, 0.0f, 100.0f);

            // This one changes what is written into the accumulation buffer, so it does reset it
            resetAccum |= ImGui::Checkbox("Show truncated paths", &mDebugTruncation);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Show the fraction of paths that ran out of bounces instead of the image. Black "
                                  "means no energy is being lost to the bounce limit, white means every path is "
                                  "being cut short.");
            }

            if (resetAccum) {
                resetAccumulation();
            }
        }

        ImGui::End();
    }

    void appKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods)
    {
        App::baseKeyCallback(pWindow, key, scancode, action, mods, mpDevice, mpSwapchain, mPipelines);
    }

    void appCursorPosCallback(GLFWwindow* pWindow, double xPos, double yPos)
    {
        App::baseCursorPosCallback(pWindow, xPos, yPos);
    }

    void appMouseButtonCallback(GLFWwindow* pWindow, int button, int action, int mods)
    {
        App::baseMouseButtonCallback(pWindow, button, action, mods, mpCamera);
    }

private:
    void resetAccumulation()
    {
        mAccumulatedFrames = 0;
    }

    void createAccumulationImage()
    {
        mpAccumImage = mpDevice->createImage(mpSwapchain->getExtent().width, mpSwapchain->getExtent().height, 1, 1,
                                             VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_SFLOAT,
                                             VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        mpAccumImage->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);

        // Transition to general layout for storage image access
        VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);
        Helpers::imageBarrier(cmd, mpAccumImage->getImage(), VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        Helpers::cmdEnd(mpDevice, cmd);

        mpRayMarchingPipeline->getShader()->setResource("accumImage", mpAccumImage);
    }

    // Attaches the environment map, and the distribution used to importance sample it, to both shaders. Falls back
    // to the white placeholder until a map has been loaded.
    void setEnvironmentMapResources()
    {
        auto pEnvMap = mpEnvironmentMap ? mpEnvironmentMap : mpDummyEnvironmentMap;

        mpEnvironmentMapPipeline->getShader()->setResource("environmentMap", pEnvMap->getTexture());

        auto pShader = mpRayMarchingPipeline->getShader();
        pShader->setResource("environmentMap", pEnvMap->getTexture());
        pShader->setResource("MarginalDistribution", pEnvMap->getMarginalDistribution());
        pShader->setResource("ConditionalDistribution", pEnvMap->getConditionalDistribution());
    }

    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::shared_ptr<Pass> mpPass;
    std::vector<std::shared_ptr<Pipeline>> mPipelines;

    std::shared_ptr<Camera> mpCamera;

    std::shared_ptr<Pipeline> mpEnvironmentMapPipeline;
    std::shared_ptr<EnvironmentMap> mpEnvironmentMap;
    std::shared_ptr<EnvironmentMap> mpDummyEnvironmentMap;
    VkFormat mEnvironmentMapFormat;
    std::filesystem::path mEnvironmentMapPath;

    std::shared_ptr<Pipeline> mpRayMarchingPipeline;
    std::shared_ptr<Texture> mpVolume;
    std::filesystem::path mVolumePath;
    float mVolumeModelScale = 1.0;
    glm::vec3 mVolumeModelPosition = glm::vec3(0.0f);
    glm::mat4 mVolumeModelMatrix = glm::mat4(1.0f);

    std::shared_ptr<Image> mpAccumImage;

    std::vector<VkSpecializationMapEntry> mSpecializationMapEntries;
    VkSpecializationInfo mSpecializationInfo;
    SpecializationConstants mSpecializationConstants;

    // Path tracer settings
    bool mPathTracing = true;
    bool mAccumulate = true;
    bool mMultiScatter = true;
    bool mNextEventEstimation = true;
    bool mEnvImportanceSampling = true;
    bool mMultipleImportanceSampling = true;
    bool mRussianRoulette = true;
    bool mEnvironmentLight = true;
    bool mHenyeyGreenstein = true;
    float mPhaseG = 0.6f;
    // Water droplets absorb almost nothing in the visible range; a value like 0.95 renders a cloud as dark smoke
    float mAlbedo = 0.999f;
    float mEnvIntensity = 1.0f;
    int mMaxBounces = 256;
    int mSamplesPerFrame = 1;
    // Shadow rays deep inside a dense volume are blocked anyway, so tracing them everywhere is mostly wasted work
    int mNeeDepth = 16;

    // Display settings
    bool mTonemap = true;
    float mExposure = 1.0f;
    bool mDebugTruncation = false;

    uint32_t mAccumulatedFrames = 0;
    uint32_t mFrameCounter = 0;
    glm::mat4 mPrevView = glm::mat4(0.0f);
};

int main()
{
    VolumeViewer app = VolumeViewer();
    app.run();
    return 0;
}
