#include "Mandrill.h"

using namespace Mandrill;

class Assignment2 : public App
{
public:
    enum {
        GBUFFER_PASS = 0,
        RESOLVE_PASS = 1,
    };

    struct PushConstants {
        int renderMode;
    };

    void createInputAttachmentDescriptor()
    {
        std::vector<DescriptorDesc> descriptorDesc;
        descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, mpRenderPass->getPositionImage());
        descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, mpRenderPass->getNormalImage());
        descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, mpRenderPass->getAlbedoImage());
        mpInputAttachmentDescriptor =
            std::make_shared<Descriptor>(mpDevice, descriptorDesc, mpResolveLayout->getDescriptorSetLayouts()[0]);
    }

    Assignment2() : App("Assignment2: Deferred Shading and Shadow Maps", 1280, 720)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create render pass
        mpRenderPass = std::make_shared<Deferred>(mpDevice, mpSwapchain);

        // Create scene
        mpScene = std::make_shared<Scene>(mpDevice, mpSwapchain);

        // Create layouts for rendering the scene in first pass and resolve in the second pass
        auto pGBufferLayout = mpScene->getLayout();

        // 3 input attachments: world position, normal, albedo color
        std::vector<LayoutDesc> layoutDesc;
        layoutDesc.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutDesc.emplace_back(0, 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutDesc.emplace_back(0, 2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        mpResolveLayout = std::make_shared<Layout>(mpDevice, layoutDesc);

        // Add push constant to layout so we can set render mode in resolve pipeline
        VkPushConstantRange pushConstantRange = {
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(PushConstants),
        };
        mpResolveLayout->addPushConstantRange(pushConstantRange);

        // Create two shaders (and pipelines) for G-buffer and resolve pass respectively
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("Assignment2/GBuffer.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("Assignment2/GBuffer.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pGBufferShader = std::make_shared<Shader>(mpDevice, shaderDesc);

        shaderDesc.clear();
        shaderDesc.emplace_back("Assignment2/Resolve.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("Assignment2/Resolve.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pResolveShader = std::make_shared<Shader>(mpDevice, shaderDesc);

        // Create pipelines
        PipelineDesc pipelineDesc;
        pipelineDesc.subpass = 0;
        pipelineDesc.depthTest = VK_TRUE;
        pipelineDesc.attachmentCount = 3;
        mPipelines.emplace_back(
            std::make_shared<Pipeline>(mpDevice, pGBufferShader, pGBufferLayout, mpRenderPass, pipelineDesc));

        pipelineDesc.subpass = 1;
        pipelineDesc.depthTest = VK_FALSE;
        pipelineDesc.attachmentCount = 1;
        mPipelines.emplace_back(
            std::make_shared<Pipeline>(mpDevice, pResolveShader, mpResolveLayout, mpRenderPass, pipelineDesc));

        // Create a sampler that will be used to render materials
        mpSampler = std::make_shared<Sampler>(mpDevice);

        // Load scene
        auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\crytek_sponza\\sponza.obj");
        std::shared_ptr<Node> pNode = mpScene->addNode();
        pNode->setPipeline(mPipelines[GBUFFER_PASS]); // Render scene with first pass pipeline
        for (auto meshIndex : meshIndices) {
            pNode->addMesh(meshIndex);
        }
        // Scale down the model
        pNode->setTransform(glm::scale(glm::vec3(0.01f)));

        mpScene->setSampler(mpSampler);
        mpScene->compile();
        mpScene->syncToDevice();

        // Activate back-face culling for G-buffer pass
        mPipelines[GBUFFER_PASS]->setCullMode(VK_CULL_MODE_BACK_BIT);

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Create descriptor for resolve pass input attachments
        createInputAttachmentDescriptor();

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
            createInputAttachmentDescriptor();
        }

        // Render scene
        mpScene->render(cmd, mpCamera);

        // Go to next subpass of the render pass
        vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the pipeline for the full-screen quad
        mPipelines[RESOLVE_PASS]->bind(cmd);

        // Bind descriptors for input attachments
        auto descriptorSet = mpInputAttachmentDescriptor->getSet(0);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[RESOLVE_PASS]->getLayout(), 0, 1,
                                &descriptorSet, 0, nullptr);

        // Push constants
        PushConstants pushConstants = {
            .renderMode = mRenderMode,
        };
        vkCmdPushConstants(cmd, mPipelines[RESOLVE_PASS]->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstants), &pushConstants);

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
        App::baseGUI(mpDevice, mpSwapchain, mpRenderPass, mPipelines);

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
        App::baseKeyCallback(window, key, scancode, action, mods, mpDevice, mpSwapchain, mpRenderPass, mPipelines);
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
    std::shared_ptr<Deferred> mpRenderPass;
    std::vector<std::shared_ptr<Pipeline>> mPipelines;

    std::shared_ptr<Layout> mpResolveLayout;
    std::shared_ptr<Descriptor> mpInputAttachmentDescriptor;

    std::shared_ptr<Sampler> mpSampler;
    std::shared_ptr<Scene> mpScene;
    std::shared_ptr<Camera> mpCamera;

    int mRenderMode = 0;
};

int main()
{
    Assignment2 app = Assignment2();
    app.run();
    return 0;
}
