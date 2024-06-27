#include "Mandrill.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3native.h"

using namespace Mandrill;

class SceneViewer : public App
{
public:
    void loadScene()
    {
        mpScene = std::make_shared<Scene>(mpDevice);
        mpScene->addNode(mScenePath);
        mpScene->update();
        mpScene->setSampler(mpSampler);
    }

    SceneViewer() : App("SceneViewer", 1920, 1080)
    {
        // Create a the Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a scene so we can access the layout, the actual scene will be loaded later
        mpScene = std::make_shared<Scene>(mpDevice);
        auto pLayout = mpScene->getLayout(1);

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
        mpCamera->update(delta);
    }

    void render() override
    {
        // Acquire frame from swapchain and prepare rasterizer
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();
        mpPipeline->frameBegin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        // Render scene
        mpScene->render(cmd, mpCamera);

        // Draw GUI
        App::drawGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpPipeline->frameEnd(cmd);
        mpSwapchain->present();
    }

    void renderGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        App::renderGUI(mpDevice, mpSwapchain, mpPipeline, mpShader);

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
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                if (GetOpenFileNameA(&ofn)) {
                    mScenePath = ofn.lpstrFile;
                }
            }

            if (mpScene) {
                ImGui::Text("Scene: %s", mScenePath.string().c_str());
                const char* modes[] = {"Diffuse", "Ambient ", "Specular", "Normal"};
                ImGui::Combo("Mode", &mRenderMode, modes, IM_ARRAYSIZE(modes));
            }
        }

        ImGui::End();
    }

private:
    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::shared_ptr<Shader> mpShader;
    std::shared_ptr<Rasterizer> mpPipeline;

    std::shared_ptr<Camera> mpCamera;

    std::shared_ptr<Sampler> mpSampler;

    std::shared_ptr<Scene> mpScene;
    std::filesystem::path mScenePath;

    int mRenderMode = 0;
};

int main()
{
    SceneViewer app = SceneViewer();
    app.run();
    return 0;
}
