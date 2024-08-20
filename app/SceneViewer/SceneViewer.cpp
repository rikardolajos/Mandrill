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
        mpScene = std::make_shared<Scene>(mpDevice);

        // Load meshes from the scene path
        auto meshIndices = mpScene->addMeshFromFile(mScenePath);

        // Add a node to the scene
        Node& node = mpScene->addNode();

        // Add all the meshes to the node
        for (auto meshIndex : meshIndices) {
            node.addMesh(meshIndex);
        }

        // Calculate and allocate buffers
        mpScene->compile();

        // Sync to GPU
        mpScene->syncToDevice();

        // Indicate which sampler should be used to handle textures
        mpScene->setSampler(mpSampler);
    }

    SceneViewer() : App("SceneViewer", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a scene so we can access the layout, the actual scene will be loaded later
        mpScene = std::make_shared<Scene>(mpDevice);
        auto pLayout = mpScene->getLayout(0);

        // Add push constant to layout so we can set render mode in shader
        VkPushConstantRange pushConstantRange = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = 2 * sizeof(int),
        };
        pLayout->addPushConstantRange(pushConstantRange);

        // Create a shader module with vertex and fragment shader
        std::vector<ShaderDescription> shaderDesc;
        shaderDesc.emplace_back("SceneViewer/VertexShader.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("SceneViewer/FragmentShader.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        mpShader = std::make_shared<Shader>(mpDevice, shaderDesc);

        // Create rasterizer pipeline with layout matching the scene
        mpPipeline = std::make_shared<Rasterizer>(mpDevice, mpSwapchain, pLayout, mpShader);

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Create a sampler that will be used to render materials
        mpSampler = std::make_shared<Sampler>(mpDevice);

        // Initialize GUI
        App::createGUI(mpDevice, mpPipeline->getRenderPass());
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
        mpPipeline->frameBegin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        // Check if camera matrix needs to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->updateAspectRatio();
        }

        vkCmdSetFrontFace(cmd, mFrontFace == 0 ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE);
        vkCmdSetCullMode(cmd, mCullMode);

        struct PushConstants {
            int renderMode;
            int discardOnZeroAlpha;
        } pushConstants = {
            .renderMode = mRenderMode,
            .discardOnZeroAlpha = mDiscardOnZeroAlpha,
        };
        vkCmdPushConstants(cmd, mpPipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof pushConstants,
                           &pushConstants);

        // Render scene
        mpScene->render(cmd, mpPipeline, mpCamera);

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpPipeline->frameEnd(cmd);
        mpSwapchain->present();
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        App::baseGUI(mpDevice, mpSwapchain, mpPipeline, mpShader);

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
            ImGui::Combo("Front face", &mFrontFace, frontFace, IM_ARRAYSIZE(frontFace));
            const char* cullModes[] = {"None", "Front face", "Back face"};
            ImGui::Combo("Cull mode", &mCullMode, cullModes, IM_ARRAYSIZE(cullModes));
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
        App::baseKeyCallback(pWindow, key, scancode, action, mods, mpDevice, mpSwapchain, mpPipeline, mpShader);
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
    std::shared_ptr<Shader> mpShader;
    std::shared_ptr<Rasterizer> mpPipeline;

    std::shared_ptr<Camera> mpCamera;
    float mCameraMoveSpeed = 1.0f;

    std::shared_ptr<Sampler> mpSampler;

    std::shared_ptr<Scene> mpScene;
    std::filesystem::path mScenePath;

    int mRenderMode = 0;
    bool mDiscardOnZeroAlpha = 0;
    int mFrontFace = 0;
    int mCullMode = 0;
};

int main()
{
    SceneViewer app = SceneViewer();
    app.run();
    return 0;
}
