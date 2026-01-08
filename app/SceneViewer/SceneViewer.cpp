#include "Mandrill.h"

using namespace Mandrill;

class SceneViewer : public App
{
public:
    enum PipelineType {
        PIPELINE_FILL,
        PIPELINE_LINE,
    };

    struct PushConstants {
        glm::vec3 lineColor;
        int _pad0;
        int renderMode;
        int discardOnZeroAlpha;
    };

    void loadScene()
    {
        // Create a new scene
        mpScene = mpDevice->createScene();

        // Load meshes from the scene path
        auto meshIndices = mpScene->addMeshFromFile(mScenePath);

        // Add a node to the scene
        mpNode = mpScene->addNode();
        mpNode->setPipeline(mPipelines[PIPELINE_FILL]);
        mpNode->setTransform(glm::mat4(mSceneScale));

        // Add all the meshes to the node
        for (auto meshIndex : meshIndices) {
            mpNode->addMesh(meshIndex);
        }

        // Indicate which sampler should be used to handle textures
        mpScene->setSampler(mpSampler);

        // Calculate and allocate buffers
        mpScene->compile(mpSwapchain->getFramesInFlightCount());

        // Create descriptors
        mpScene->createDescriptors(mPipelines[PIPELINE_FILL]->getShader()->getDescriptorSetLayouts(),
                                   mpSwapchain->getFramesInFlightCount());

        // Sync to GPU
        mpScene->syncToDevice();
    }

    SceneViewer() : App("SceneViewer", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight (default)
        mpSwapchain = mpDevice->createSwapchain();

        // Create a pass with 1 color attachment, depth attachment and multisampling
        uint32_t colorAttachmentCount = 1;
        bool depthAttachment = true;
        mpPass = mpDevice->createPass(mpSwapchain->getExtent(), mpSwapchain->getImageFormat(), colorAttachmentCount,
                                      depthAttachment, mpDevice->getSampleCount());

        // Create a shader module with vertex and fragment shader
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("SceneViewer/VertexShader.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("SceneViewer/FragmentShader.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pShader = mpDevice->createShader(shaderDesc);

        // Create a pipeline filled polygon rendering
        mPipelines.emplace_back(mpDevice->createPipeline(mpPass, pShader, PipelineDesc()));

        // Create a pipeline for line rendering
        PipelineDesc pipelineDesc;
        pipelineDesc.polygonMode = VK_POLYGON_MODE_LINE;
        mPipelines.emplace_back(mpDevice->createPipeline(mpPass, pShader, pipelineDesc));

        // Setup camera
        mpCamera = mpDevice->createCamera(mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);
        mpCamera->createDescriptor(VK_SHADER_STAGE_VERTEX_BIT);

        // Create a sampler that will be used to render materials
        mpSampler = mpDevice->createSampler();

        // Start with an empty scene
        mpScene = mpDevice->createScene();

        // Initialize GUI
        App::createGUI(mpDevice, mpPass);
    }

    ~SceneViewer()
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

        PushConstants pushConstants = {
            .renderMode = mRenderMode,
            .discardOnZeroAlpha = mDiscardOnZeroAlpha,
        };
        vkCmdPushConstants(cmd, mPipelines[PIPELINE_FILL]->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof pushConstants, &pushConstants);

        // Render scene
        mpScene->render(cmd, mpCamera, mpSwapchain->getInFlightIndex());

        // Render lines
        if (mDrawPolygonLines) {
            // Switch to line rendering
            for (auto& node : mpScene->getNodes()) {
                node.setPipeline(mPipelines[PIPELINE_LINE]);
            }

            PushConstants pushConstants = {
                .lineColor = mLineColor,
                .renderMode = 9,
                .discardOnZeroAlpha = mDiscardOnZeroAlpha,
            };
            vkCmdPushConstants(cmd, mPipelines[PIPELINE_LINE]->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof pushConstants, &pushConstants);

            mPipelines[PIPELINE_LINE]->setLineWidth(mLineWidth);

            mpScene->render(cmd, mpCamera, mpSwapchain->getInFlightIndex());

            // Reset pipeline
            for (auto& node : mpScene->getNodes()) {
                node.setPipeline(mPipelines[PIPELINE_FILL]);
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

        if (ImGui::Begin("Scene Viewer")) {
            if (ImGui::Button("Load")) {
                mScenePath = OpenFile(mpWindow, "All\0*.*\0Wavefront Object (*.obj)\0*.OBJ\0");
                if (!mScenePath.empty()) {
                    loadScene();
                }
            }

            ImGui::Text("Scene: %s", mScenePath.string().c_str());
            const char* renderModes[] = {
                "Diffuse",  "Specular",  "Ambient",
                "Emission", "Shininess", "Index of refraction",
                "Opacity",  "Normal",    "Texture coordinates",
            };
            ImGui::Combo("Render mode", &mRenderMode, renderModes, IM_ARRAYSIZE(renderModes));
            const char* frontFace[] = {"Counter clockwise", "Clockwise"};
            if (ImGui::Combo("Front face", &mFrontFace, frontFace, IM_ARRAYSIZE(frontFace))) {
                mPipelines[PIPELINE_FILL]->setFrontFace(mFrontFace == 0 ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                                                        : VK_FRONT_FACE_CLOCKWISE);
            }
            const char* cullModes[] = {"None", "Front face", "Back face"};
            if (ImGui::Combo("Cull mode", &mCullMode, cullModes, IM_ARRAYSIZE(cullModes))) {
                mPipelines[PIPELINE_FILL]->setCullMode(static_cast<VkCullModeFlagBits>(mCullMode));
            };
            ImGui::Checkbox("Draw polyon lines", &mDrawPolygonLines);
            if (mDrawPolygonLines) {
                ImGui::ColorEdit3("Line color", &mLineColor.x);
                ImGui::SliderFloat("Line width", &mLineWidth, 1.0f, 10.0f);
            }

            bool newSampler = false;
            const char* magFilters[] = {"Linear", "Nearest"};
            if (ImGui::Combo("Mag filter", &mMagFilter, magFilters, IM_ARRAYSIZE(magFilters))) {
                newSampler = true;
            }
            const char* minFilters[] = {"Linear", "Nearest"};
            if (ImGui::Combo("Min filter", &mMinFilter, minFilters, IM_ARRAYSIZE(minFilters))) {
                newSampler = true;
            }
            const char* mipModes[] = {"Linear", "Nearest"};
            if (ImGui::Combo("Mip mode", &mMipMode, mipModes, IM_ARRAYSIZE(mipModes))) {
                newSampler = true;
            }

            if (newSampler) {
                mpSampler = std::make_shared<Sampler>(mpDevice, mMagFilter ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
                                                      mMinFilter ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
                                                      mMipMode ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                                                               : VK_SAMPLER_MIPMAP_MODE_LINEAR);
                mpScene->setSampler(mpSampler);

                if (!mScenePath.empty()) {
                    mpScene->compile(mpSwapchain->getFramesInFlightCount());
                    mpScene->createDescriptors(mPipelines[PIPELINE_FILL]->getShader()->getDescriptorSetLayouts(),
                                               mpSwapchain->getFramesInFlightCount());
                    mpScene->syncToDevice();
                }
            }

            ImGui::Checkbox("Discard pixel if diffuse alpha channel is 0", &mDiscardOnZeroAlpha);

            if (ImGui::SliderFloat("Scene scale", &mSceneScale, 0.01f, 10.0f)) {
                mpNode->setTransform(glm::scale(glm::vec3(mSceneScale)));
            }

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

    std::shared_ptr<Sampler> mpSampler;

    std::filesystem::path mScenePath;
    std::shared_ptr<Scene> mpScene;
    std::shared_ptr<Node> mpNode;
    float mSceneScale = 1.0f;

    int mRenderMode = 0;
    bool mDiscardOnZeroAlpha = false;
    bool mDrawPolygonLines = false;
    glm::vec3 mLineColor = glm::vec3(0.0f, 1.0f, 0.0f);
    float mLineWidth = 1.0f;
    int mFrontFace = 0;
    int mCullMode = 0;
    int mMagFilter = 0;
    int mMinFilter = 0;
    int mMipMode = 0;
};

int main()
{
    SceneViewer app = SceneViewer();
    app.run();
    return 0;
}
