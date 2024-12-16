#include "Mandrill.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

using namespace Mandrill;

class SceneViewer : public App
{
public:
    enum PipelineType {
        PIPELINE_FILL,
        PIPELINE_LINE,
    };

    struct PushConstants {
        int renderMode;
        int discardOnZeroAlpha;
        alignas(16) glm::vec3 lineColor;
    };

    void loadScene()
    {
        // Create a new scene
        mpScene = std::make_shared<Scene>(mpDevice, mpSwapchain);

        // Load meshes from the scene path
        auto meshIndices = mpScene->addMeshFromFile(mScenePath);

        // Add a node to the scene
        std::shared_ptr<Node> pNode = mpScene->addNode();
        pNode->setPipeline(mPipelines[PIPELINE_FILL]);

        // Add all the meshes to the node
        for (auto meshIndex : meshIndices) {
            pNode->addMesh(meshIndex);
        }

        // Indicate which sampler should be used to handle textures
        mpScene->setSampler(mpSampler);

        // Calculate and allocate buffers
        mpScene->compile();

        // Sync to GPU
        mpScene->syncToDevice();
    }

    SceneViewer() : App("SceneViewer", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a scene so we can access the layout, the actual scene will be loaded later
        mpScene = std::make_shared<Scene>(mpDevice, mpSwapchain);
        auto pLayout = mpScene->getLayout();

        // Create rasterizer render pass with layout matching the scene
        mpRenderPass = std::make_shared<Rasterizer>(mpDevice, mpSwapchain);

        // Add push constant to layout so we can set render mode in shader
        VkPushConstantRange pushConstantRange = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(PushConstants),
        };
        pLayout->addPushConstantRange(pushConstantRange);

        // Create a shader module with vertex and fragment shader
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("SceneViewer/VertexShader.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("SceneViewer/FragmentShader.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        std::shared_ptr<Shader> pShader = std::make_shared<Shader>(mpDevice, shaderDesc);

        // Create a pipeline filled polygon rendering
        mPipelines.emplace_back(std::make_shared<Pipeline>(mpDevice, pShader, pLayout, mpRenderPass));

        // Create a pipeline for line rendering
        PipelineDesc pipelineDesc;
        pipelineDesc.polygonMode = VK_POLYGON_MODE_LINE;
        mPipelines.emplace_back(
            std::make_shared<Pipeline>(mpDevice, pShader, pLayout, mpRenderPass, pipelineDesc));

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Create a sampler that will be used to render materials
        mpSampler = std::make_shared<Sampler>(mpDevice);

        // Initialize GUI
        App::createGUI(mpDevice, mpRenderPass->getRenderPass(), mpDevice->getSampleCount());
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
        // Acquire frame from swapchain and prepare rasterizer
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();
        mpRenderPass->frameBegin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        // Check if camera matrix needs to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->updateAspectRatio();
        }

        PushConstants pushConstants = {
            .renderMode = mRenderMode,
            .discardOnZeroAlpha = mDiscardOnZeroAlpha,
        };
        vkCmdPushConstants(cmd, mPipelines[PIPELINE_FILL]->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof pushConstants, &pushConstants);

        // Render scene
        mpScene->render(cmd, mpCamera);

        // Render lines
        if (mDrawPolygonLines) {
            // Switch to line rendering
            for (auto& node : mpScene->getNodes()) {
                node.setPipeline(mPipelines[PIPELINE_LINE]);
            }

            PushConstants pushConstants = {
                .renderMode = 9,
                .discardOnZeroAlpha = mDiscardOnZeroAlpha,
                .lineColor = mLineColor,
            };
            vkCmdPushConstants(cmd, mPipelines[PIPELINE_LINE]->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof pushConstants, &pushConstants);

            mPipelines[PIPELINE_LINE]->setLineWidth(mLineWidth);

            mpScene->render(cmd, mpCamera);

            // Reset pipeline
            for (auto& node : mpScene->getNodes()) {
                node.setPipeline(mPipelines[PIPELINE_FILL]);
            }
        }

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpRenderPass->frameEnd(cmd);
        mpSwapchain->present();
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        App::baseGUI(mpDevice, mpSwapchain, mpRenderPass, mPipelines);

        if (ImGui::Begin("Scene Viewer")) {
            if (ImGui::Button("Load")) {
                OPENFILENAME ofn;
                TCHAR szFile[260] = {0};

                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = glfwGetWin32Window(mpWindow);
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFilter = "All\0*.*\0Wavefront Object (*.obj)\0*.OBJ\0";
                ofn.nFilterIndex = 2;
                ofn.lpstrFileTitle = NULL;
                ofn.nMaxFileTitle = 0;
                ofn.lpstrInitialDir = NULL;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

                if (GetOpenFileNameA(&ofn)) {
                    mScenePath = ofn.lpstrFile;
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
                mpScene->compile();
                mpScene->syncToDevice();
            }

            ImGui::Checkbox("Discard pixel if diffuse alpha channel is 0", &mDiscardOnZeroAlpha);

            if (ImGui::SliderFloat("Camera move speed", &mCameraMoveSpeed, 0.1f, 100.0f)) {
                mpCamera->setMoveSpeed(mCameraMoveSpeed);
            }
        }

        ImGui::End();
    }

    void appKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods)
    {
        // Invoke the base application's keyboard commands
        App::baseKeyCallback(pWindow, key, scancode, action, mods, mpDevice, mpSwapchain, mpRenderPass, mPipelines);
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
    std::shared_ptr<Rasterizer> mpRenderPass;
    std::vector<std::shared_ptr<Pipeline>> mPipelines;

    std::shared_ptr<Camera> mpCamera;
    float mCameraMoveSpeed = 1.0f;

    std::shared_ptr<Sampler> mpSampler;

    std::shared_ptr<Scene> mpScene;
    std::filesystem::path mScenePath;

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
