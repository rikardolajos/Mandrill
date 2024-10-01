#include "Mandrill.h"

using namespace Mandrill;

class Assignment2 : public App
{
public:
    Assignment2() : App("Assignment2: Deferred Shading and Shadow Maps", 1280, 720)
    {
        // Create a Vulkan instance and device
        mpDevice = make_ptr<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = make_ptr<Swapchain>(mpDevice, 2);

        // Create a sampler that will be used to render materials
        mpSampler = make_ptr<Sampler>(mpDevice);

        // Create and load scene
        mpScene = make_ptr<Scene>(mpDevice, mpSwapchain);
        auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\crytek_sponza\\sponza.obj");
        ptr<Node> pNode = mpScene->addNode();
        for (auto meshIndex : meshIndices) {
            pNode->addMesh(meshIndex);
        }
        // Scale down the model
        pNode->setTransform(glm::scale(glm::vec3(0.01f)));

        mpScene->setSampler(mpSampler);
        mpScene->compile();
        mpScene->syncToDevice();

        // Deferred render pass requires 2 passes (G-buffer and resolve)
        std::vector<ShaderDesc> shaderDesc1;
        std::vector<ShaderDesc> shaderDesc2;
        shaderDesc1.emplace_back("Assignment2/GBuffer.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc1.emplace_back("Assignment2/GBuffer.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        shaderDesc2.emplace_back("Assignment2/Resolve.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc2.emplace_back("Assignment2/Resolve.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        mpShaders.push_back(make_ptr<Shader>(mpDevice, shaderDesc1));
        mpShaders.push_back(make_ptr<Shader>(mpDevice, shaderDesc2));

        // Create layouts for rendering the scene in first pass and resolve in the second pass
        std::vector<std::shared_ptr<Layout>> layouts;
        layouts.push_back(mpScene->getLayout()); // First pass

        // 3 input attachments: world position, normal, albedo color
        std::vector<LayoutDesc> layoutDesc;
        layoutDesc.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutDesc.emplace_back(0, 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutDesc.emplace_back(0, 2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pResolveLayout = make_ptr<Layout>(mpDevice, layoutDesc);

        // Add push constant to layout so we can set render mode in shader
        VkPushConstantRange pushConstantRange = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = 1 * sizeof(int),
        };
        pResolveLayout->addPushConstantRange(pushConstantRange);
        layouts.push_back(pResolveLayout); // Second pass

        // Create render pass
        RenderPassDesc renderPassDesc(mpShaders, layouts);
        mpRenderPass = make_ptr<Deferred>(mpDevice, mpSwapchain, renderPassDesc);

        // Setup camera
        mpCamera = make_ptr<Camera>(mpDevice, mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Initialize GUI
        App::createGUI(mpDevice, mpRenderPass->getRenderPass(), VK_SAMPLE_COUNT_1_BIT, 1);
    }

    ~Assignment2()
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
        mpRenderPass->frameBegin(cmd, glm::vec4(0.2f, 0.6f, 1.0f, 1.0f));

        // Check if camera matrix needs to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->updateAspectRatio();
        }

        // Sponza's triangles are clockwise
        vkCmdSetFrontFace(cmd, VK_FRONT_FACE_CLOCKWISE);
        vkCmdSetCullMode(cmd, VK_CULL_MODE_BACK_BIT);

        // Render scene
        mpScene->render(cmd, mpCamera, mpRenderPass->getPipelineLayout(0));

        // Go to next subpass
        mpRenderPass->nextSubpass(cmd);

        // Push constants
        struct PushConstants {
            int renderMode;
        } pushConstants = {
            .renderMode = mRenderMode,
        };
        vkCmdPushConstants(cmd, mpRenderPass->getPipelineLayout(1), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof pushConstants, &pushConstants);

        // Render full-screen quad to resolve final composition
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpRenderPass->frameEnd(cmd);
        mpSwapchain->present();
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        // Render the base GUI, the menu bar with it's subwindows
        App::baseGUI(mpDevice, mpSwapchain, mpRenderPass, mpShaders);

        if (ImGui::Begin("Assignment 2")) {
            const char* renderModes[] = {
                "Resolved",
                "Position",
                "Normal",
                "Albedo",
            };
            ImGui::Combo("Render mode", &mRenderMode, renderModes, IM_ARRAYSIZE(renderModes));
        }

        ImGui::End();
    }

    void appKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        App::baseKeyCallback(window, key, scancode, action, mods, mpDevice, mpSwapchain, mpRenderPass, mpShaders);
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
    std::vector<ptr<Shader>> mpShaders;
    ptr<Deferred> mpRenderPass;

    ptr<Descriptor> mpInputAttachmentDescriptor;

    ptr<Sampler> mpSampler;
    ptr<Scene> mpScene;
    ptr<Camera> mpCamera;

    int mRenderMode = 0;
};

int main()
{
    Assignment2 app = Assignment2();
    app.run();
    return 0;
}
