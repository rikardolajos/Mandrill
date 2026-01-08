#include "Mandrill.h"

using namespace Mandrill;

class VolumeViewer : public App
{
public:
    struct PushConstant {
        glm::mat4 inverseModel;
        glm::vec3 gridMin;
        float _padding0;
        glm::vec3 gridMax;
        float _padding1;
        glm::vec2 viewport;
    };

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
        pipelineDesc.blendEnable = VK_FALSE;

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
        pipelineDesc.blendEnable = VK_TRUE;
        mpRayMarchingPipeline = mpDevice->createPipeline(mpPass, pRayMarchShader, pipelineDesc);

        mPipelines = {mpEnvironmentMapPipeline, mpRayMarchingPipeline};

        // Setup camera
        mpCamera = mpDevice->createCamera(mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);
        mpCamera->createDescriptor(VK_SHADER_STAGE_FRAGMENT_BIT);

        // Create sampler for environment map
        mpEnvironmentMapSampler = mpDevice->createSampler();

        // Create a sampler that will be used to sample volume
        mpVolumeSampler = mpDevice->createSampler(
            VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

        // Initialize GUI
        App::createGUI(mpDevice, mpPass);
    }

    ~VolumeViewer()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta)
    {
        if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
            mpCamera->update(delta, getCursorDelta());
        }
    }

    void render() override
    {
        // Check if camera matrix and attachments need to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->updateAspectRatio();
            mpPass->update(mpSwapchain->getExtent());
        }

        // Acquire frame from swapchain and prepare rasterizer
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();
        mpPass->begin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        //  Camera descriptor
        VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
        uint32_t cameraDescriptorOffset = static_cast<uint32_t>(Helpers::alignTo(sizeof(CameraMatrices), alignment) *
                                                                mpSwapchain->getInFlightIndex());
        mpCamera->getDescriptor()->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpEnvironmentMapPipeline->getLayout(), 0,
                                        cameraDescriptorOffset);

        // Push constants
        glm::vec3 volumeDim(1.0f);
        if (mpVolume) {
            volumeDim = glm::vec3(mpVolume->getImage()->getWidth(), mpVolume->getImage()->getHeight(),
                                  mpVolume->getImage()->getDepth());
        }
        glm::vec3 gridMin = mVolumeModelPosition - (mVolumeModelScale * volumeDim / 2.0f);
        glm::vec3 gridMax = gridMin + mVolumeModelScale * volumeDim;
        PushConstant pushConstant = {
            .inverseModel = glm::inverse(mVolumeModelMatrix),
            .gridMin = gridMin,
            .gridMax = gridMax,
            .viewport = glm::vec2(mWidth, mHeight),
        };
        vkCmdPushConstants(cmd, mpEnvironmentMapPipeline->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof pushConstant, &pushConstant);

        // Render environment map
        if (mpEnvironmentMap) {
            mpEnvironmentMapPipeline->bind(cmd);
            mpEnvironmentMapDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                             mpEnvironmentMapPipeline->getLayout(), 1);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }

        // Render volume
        if (mpVolume) {
            mpRayMarchingPipeline->bind(cmd);
            mpVolumeDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpRayMarchingPipeline->getLayout(), 1);
            vkCmdDraw(cmd, 3, 1, 0, 0);
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
            ImGui::SeparatorText("Volume");
            if (ImGui::Button("Load##Volume")) {
                mVolumePath = OpenFile(mpWindow, "OpenVDB file (*.vdb)\0*.VDB\0All (*.*)\0*.*\0");
                if (!mVolumePath.empty()) {
                    mpVolume = std::make_shared<Texture>(mpDevice, TextureType::Texture3D, VK_FORMAT_R32_SFLOAT,
                                                         mVolumePath, false);
                    mpVolume->setSampler(mpVolumeSampler);

                    std::vector<DescriptorDesc> desc;
                    desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mpVolume);
                    mpVolumeDescriptor =
                        mpDevice->createDescriptor(desc, mpRayMarchingPipeline->getShader()->getDescriptorSetLayout(1));

                    glm::vec3 volumeDim = glm::vec3(mpVolume->getImage()->getWidth(), mpVolume->getImage()->getHeight(),
                                                    mpVolume->getImage()->getDepth());
                    mVolumeModelScale = 1.0f / glm::max(volumeDim.x, glm::max(volumeDim.y, volumeDim.z));
                }
            }
            ImGui::SameLine();
            ImGui::Text(mVolumePath.string().c_str());
            bool recreatePipeline = false;
            recreatePipeline |= ImGui::DragFloat("Density", &mSpecializationConstants.density, 0.01f, 0.0f, 100.0f);

            bool newModelMatrix = false;
            newModelMatrix |= ImGui::DragFloat("Scale", &mVolumeModelScale, 0.01f);
            newModelMatrix |= ImGui::DragFloat3("Position", &mVolumeModelPosition.x, 0.01f);
            if (newModelMatrix) {
                mVolumeModelMatrix = glm::scale(glm::vec3(mVolumeModelScale));
                mVolumeModelMatrix = glm::translate(mVolumeModelMatrix, mVolumeModelPosition);
            }

            ImGui::SeparatorText("Ray Marcher");
            recreatePipeline |= ImGui::DragInt("Max steps", &mSpecializationConstants.maxSteps, 1.0f);

            recreatePipeline |=
                ImGui::DragFloat("Step size", &mSpecializationConstants.stepSize, 0.0001f, 0.0f, 0.0f, "%.4f");

            if (recreatePipeline) {
                mpRayMarchingPipeline->recreate();
            }

            ImGui::SeparatorText("Environment Map");
            if (ImGui::Button("Load##EnvMap")) {
                mEnvironmentMapPath =
                    OpenFile(mpWindow, "Supported image files (*.hdr, *.png)\0*.HDR;*.PNG\0All (*.*)\0*.*\0");
                if (!mEnvironmentMapPath.empty()) {
                    mpEnvironmentMap = std::make_shared<Texture>(mpDevice, TextureType::Texture2D,
                                                                 VK_FORMAT_R8G8B8A8_UNORM, mEnvironmentMapPath, false);
                    mpEnvironmentMap->setSampler(mpEnvironmentMapSampler);

                    std::vector<DescriptorDesc> desc;
                    desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mpEnvironmentMap);
                    mpEnvironmentMapDescriptor = mpDevice->createDescriptor(
                        desc, mpEnvironmentMapPipeline->getShader()->getDescriptorSetLayout(1));
                }
            }
            ImGui::SameLine();
            ImGui::Text(mEnvironmentMapPath.string().c_str());
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
    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::shared_ptr<Pass> mpPass;
    std::vector<std::shared_ptr<Pipeline>> mPipelines;

    std::shared_ptr<Camera> mpCamera;

    std::shared_ptr<Pipeline> mpEnvironmentMapPipeline;
    std::shared_ptr<Texture> mpEnvironmentMap;
    std::filesystem::path mEnvironmentMapPath;
    std::shared_ptr<Sampler> mpEnvironmentMapSampler;
    std::shared_ptr<Descriptor> mpEnvironmentMapDescriptor;

    std::shared_ptr<Pipeline> mpRayMarchingPipeline;
    std::shared_ptr<Texture> mpVolume;
    std::filesystem::path mVolumePath;
    std::shared_ptr<Sampler> mpVolumeSampler;
    float mVolumeModelScale = 1.0;
    glm::vec3 mVolumeModelPosition = glm::vec3(0.0f);
    glm::mat4 mVolumeModelMatrix = glm::mat4(1.0f);
    std::shared_ptr<Descriptor> mpVolumeDescriptor;

    std::vector<VkSpecializationMapEntry> mSpecializationMapEntries;
    VkSpecializationInfo mSpecializationInfo;
    SpecializationConstants mSpecializationConstants;
};

int main()
{
    VolumeViewer app = VolumeViewer();
    app.run();
    return 0;
}
