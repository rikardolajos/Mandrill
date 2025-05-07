#include "Mandrill.h"

using namespace Mandrill;

class VolumeViewer : public App
{
public:
    struct PushConstantEnv {
        glm::vec2 viewport;
    };

    VolumeViewer() : App("VolumeViewer", 1920, 1080)
    {
        // Create a Vulkan instance and device
        std::vector<const char*> extensions = {
            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        };
        mpDevice = std::make_shared<Device>(mpWindow, extensions);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a layout (push descriptor):
        // Set 0, binding 0: Camera matrices
        // Set 0, binding 1: Environment map texture
        // Set 0, binding 2: Volume density 3D texture
        std::vector<LayoutDesc> layoutDesc;
        layoutDesc.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
        layoutDesc.emplace_back(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS);
        layoutDesc.emplace_back(0, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS);

        auto pLayout =
            std::make_shared<Layout>(mpDevice, layoutDesc, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
        VkPushConstantRange pushConstantRange = {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset = 0,
            .size = sizeof(PushConstantEnv),
        };
        pLayout->addPushConstantRange(pushConstantRange);

        // Create a pass with 1 color attachment, depth attachment and multisampling
        mpPass = std::make_shared<Pass>(mpDevice, mpSwapchain->getExtent(), mpSwapchain->getImageFormat(), 1, true,
                                        mpDevice->getSampleCount());

        // Prepare vertex binding and attribute descriptions with empty vectors (only fullscreen triangles are used)
        std::vector<VkVertexInputBindingDescription> emptyBindingDescription;
        std::vector<VkVertexInputAttributeDescription> emptyAttributeDescription;
        PipelineDesc pipelineDesc = PipelineDesc(emptyBindingDescription, emptyAttributeDescription);

        // Create a pipeline for environment map
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("VolumeViewer/Fullscreen.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("VolumeViewer/Environment.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        std::shared_ptr<Shader> pShader = std::make_shared<Shader>(mpDevice, shaderDesc);
        mpEnvironmentMapPipeline = std::make_shared<Pipeline>(mpDevice, mpPass, pLayout, pShader, pipelineDesc);

        // Create a pipeline for ray marching
        shaderDesc.clear();
        shaderDesc.emplace_back("VolumeViewer/Fullscreen.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("VolumeViewer/RayMarcher.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        pShader = std::make_shared<Shader>(mpDevice, shaderDesc);
        mpRayMarchingPipeline = std::make_shared<Pipeline>(mpDevice, mpPass, pLayout, pShader, pipelineDesc);

        mPipelines = {mpEnvironmentMapPipeline, mpRayMarchingPipeline};

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(1.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Create sampler for environment map
        mpEnvironmentMapSampler = std::make_shared<Sampler>(mpDevice);

        // Create a sampler that will be used to sample volume
        mpVolumeSampler =
            std::make_shared<Sampler>(mpDevice, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

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

        // Push descriptors
        std::vector<VkWriteDescriptorSet> writes = {
            mpCamera->getWriteDescriptor(0),
        };
        if (mpEnvironmentMap) {
            writes.push_back(mpEnvironmentMap->getWriteDescriptor(1));
        }
        if (mpVolume) {
            writes.push_back(mpVolume->getWriteDescriptor(2));
        }
        vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpEnvironmentMapPipeline->getLayout(), 0,
                                  count(writes), writes.data());

        // Push constants
        PushConstantEnv pushConstant = {
            .viewport = glm::vec2(mWidth, mHeight),
        };
        vkCmdPushConstants(cmd, mpEnvironmentMapPipeline->getLayout(), VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                           sizeof(PushConstantEnv), &pushConstant);

        // Render environment map
        if (mpEnvironmentMap) {
            mpEnvironmentMapPipeline->bind(cmd);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }

        // Render volume
        if (mpVolume) {
            mpRayMarchingPipeline->bind(cmd);
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
                    mpVolume = std::make_shared<Texture>(mpDevice, Texture::Type::Texture3D, VK_FORMAT_R32_SFLOAT,
                                                         mVolumePath, false);
                    mpVolume->setSampler(mpVolumeSampler);
                }
            }
            ImGui::SameLine();
            ImGui::Text(mVolumePath.string().c_str());

            ImGui::SeparatorText("Environment Map");
            if (ImGui::Button("Load##EnvMap")) {
                mEnvironmentMapPath =
                    OpenFile(mpWindow, "Supported image files (*.hdr, *.png)\0*.HDR;*.PNG\0All (*.*)\0*.*\0");
                if (!mEnvironmentMapPath.empty()) {
                    mpEnvironmentMap = std::make_shared<Texture>(mpDevice, Texture::Type::Texture2D,
                                                                 VK_FORMAT_R8G8B8A8_UNORM, mEnvironmentMapPath, false);
                    mpEnvironmentMap->setSampler(mpEnvironmentMapSampler);
                }
            }
            ImGui::SameLine();
            ImGui::Text(mEnvironmentMapPath.string().c_str());

            if (ImGui::SliderFloat("Camera move speed", &mCameraMoveSpeed, 0.1f, 100.0f)) {
                mpCamera->setMoveSpeed(mCameraMoveSpeed);
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
    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::shared_ptr<Pass> mpPass;
    std::vector<std::shared_ptr<Pipeline>> mPipelines;

    std::shared_ptr<Camera> mpCamera;
    float mCameraMoveSpeed = 1.0f;

    std::shared_ptr<Pipeline> mpEnvironmentMapPipeline;
    std::shared_ptr<Texture> mpEnvironmentMap;
    std::filesystem::path mEnvironmentMapPath;
    std::shared_ptr<Sampler> mpEnvironmentMapSampler;

    std::shared_ptr<Pipeline> mpRayMarchingPipeline;
    std::shared_ptr<Texture> mpVolume;
    std::filesystem::path mVolumePath;
    std::shared_ptr<Sampler> mpVolumeSampler;
};

int main()
{
    VolumeViewer app = VolumeViewer();
    app.run();
    return 0;
}
