#include "Mandrill.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

using namespace Mandrill;

class SceneViewer : public App
{
public:
    void loadScene()
    {
        // Create a new scene
        mpScene = make_ptr<Scene>(mpDevice, mpSwapchain);

        // Load meshes from the scene path
        auto meshIndices = mpScene->addMeshFromFile(mScenePath);

        // Add a node to the scene
        ptr<Node> pNode = mpScene->addNode();
        pNode->setPipeline(mpPipeline);

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
        mpDevice = make_ptr<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = make_ptr<Swapchain>(mpDevice, 2);

        // Create a scene so we can access the layout, the actual scene will be loaded later
        mpScene = make_ptr<Scene>(mpDevice, mpSwapchain);
        auto pLayout = mpScene->getLayout();

        // Create rasterizer render pass with layout matching the scene
        mpRenderPass = make_ptr<Rasterizer>(mpDevice, mpSwapchain);

        // Add push constant to layout so we can set render mode in shader
        VkPushConstantRange pushConstantRange = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = 2 * sizeof(int),
        };
        pLayout->addPushConstantRange(pushConstantRange);

        // Create a shader module with vertex and fragment shader
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("SceneViewer/VertexShader.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("SceneViewer/FragmentShader.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        ptr<Shader> pShader = make_ptr<Shader>(mpDevice, shaderDesc);

        // Create a pipeline that will render with the given shader
        mpPipeline = std::make_shared<Pipeline>(mpDevice, pShader, pLayout, mpRenderPass);

        // Setup camera
        mpCamera = make_ptr<Camera>(mpDevice, mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Create a sampler that will be used to render materials
        mpSampler = make_ptr<Sampler>(mpDevice);

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

        struct PushConstants {
            int renderMode;
            int discardOnZeroAlpha;
        } pushConstants = {
            .renderMode = mRenderMode,
            .discardOnZeroAlpha = mDiscardOnZeroAlpha,
        };
        vkCmdPushConstants(cmd, mpPipeline->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof pushConstants, &pushConstants);

        // Render scene
        mpScene->render(cmd, mpCamera);

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpRenderPass->frameEnd(cmd);
        mpSwapchain->present();
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        App::baseGUI(mpDevice, mpSwapchain, mpRenderPass, mpPipeline);

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
                mpPipeline->setFrontFace(mFrontFace == 0 ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE);
            }
            const char* cullModes[] = {"None", "Front face", "Back face"};
            if (ImGui::Combo("Cull mode", &mCullMode, cullModes, IM_ARRAYSIZE(cullModes))) {
                mpPipeline->setCullMode(static_cast<VkCullModeFlagBits>(mCullMode));
            };

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
                mpSampler =
                    make_ptr<Sampler>(mpDevice, mMagFilter ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
                                      mMinFilter ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
                                      mMipMode ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR);
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
        App::baseKeyCallback(pWindow, key, scancode, action, mods, mpDevice, mpSwapchain, mpRenderPass, mpPipeline);
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
    ptr<Device> mpDevice;
    ptr<Swapchain> mpSwapchain;
    ptr<Rasterizer> mpRenderPass;
    ptr<Pipeline> mpPipeline;

    ptr<Camera> mpCamera;
    float mCameraMoveSpeed = 1.0f;

    ptr<Sampler> mpSampler;

    ptr<Scene> mpScene;
    std::filesystem::path mScenePath;

    int mRenderMode = 0;
    bool mDiscardOnZeroAlpha = 0;
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
