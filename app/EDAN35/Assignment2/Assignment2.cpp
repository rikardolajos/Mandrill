#include "Mandrill.h"

using namespace Mandrill;

class Assignment2 : public App
{
public:
    Assignment2() : App("Assignment2: Deferred Shading and Shadow Maps", 1280, 720)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a sampler that will be used to render materials
        mpSampler = std::make_shared<Sampler>(mpDevice);

        // Create and load scene
        mpScene = std::make_shared<Scene>(mpDevice);
        auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\crytek_sponza\\sponza.obj");
        Node& node = mpScene->addNode();
        for (auto meshIndex : meshIndices) {
            node.addMesh(meshIndex);
        }
        mpScene->compile();
        mpScene->syncToDevice();
        mpScene->setSampler(mpSampler);

        // Deferred render pass requires 2 passes (G-buffer and resolve)
        mpShaders.resize(2);
        std::vector<ShaderDescription> shaderDesc1;
        std::vector<ShaderDescription> shaderDesc2;
        shaderDesc1.emplace_back("Assignment2/GBuffer.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc1.emplace_back("Assignment2/GBuffer.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        shaderDesc2.emplace_back("Assignment2/Resolve.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc2.emplace_back("Assignment2/Resolve.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        mpShaders.emplace_back(mpDevice, shaderDesc1);
        mpShaders.emplace_back(mpDevice, shaderDesc2);

        // Create layouts for rendering the scene in first pass and resolve in the second pass
        std::vector<std::shared_ptr<Layout>> layouts;
        layouts.push_back(mpScene->getLayout(0));

        // 3 input attachments: world position, normals, ambient color
        std::vector<LayoutDescription> layoutDesc;
        layoutDesc.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutDesc.emplace_back(0, 1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        layoutDesc.emplace_back(0, 2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
        layouts.emplace_back(mpDevice, layoutDesc, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

        // Create render pass
        mpRenderPass = std::make_shared<Deferred>(mpDevice, mpSwapchain, layouts, mpShaders);

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Initialize GUI
        App::createGUI(mpDevice, mpRenderPass->getRenderPass());
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
        mpRenderPass->frameBegin(cmd, glm::vec4(0.0f, 0.4f, 0.2f, 1.0f));

        // Check if camera matrix needs to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->updateAspectRatio();
        }

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
        vkCmdPushConstants(cmd, mpRenderPass->getPipelineLayout(0), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof pushConstants, &pushConstants);

        // Push descriptors
        std::array<VkWriteDescriptorSet, 3> descriptors;

        VkDescriptorImageInfo positionInfo = mpRenderPass->getInputAttachmentInfo(0);
        VkDescriptorImageInfo normalInfo = mpRenderPass->getInputAttachmentInfo(1);
        VkDescriptorImageInfo albedoInfo = mpRenderPass->getInputAttachmentInfo(2);

        descriptors[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[0].dstBinding = 0;
        descriptors[0].descriptorCount = 1;
        descriptors[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        descriptors[0].pImageInfo = &positionInfo;

        descriptors[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[1].dstBinding = 1;
        descriptors[1].descriptorCount = 1;
        descriptors[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        descriptors[1].pImageInfo = &normalInfo;

        descriptors[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptors[2].dstBinding = 2;
        descriptors[2].descriptorCount = 1;
        descriptors[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        descriptors[2].pImageInfo = &albedoInfo;

        vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpRenderPass->getPipelineLayout(1), 0,
                                  static_cast<uint32_t>(descriptors.size()), descriptors.data());

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

        if (ImGui::Begin("Assignemnt 2")) {
            const char* renderModes[] = {
                "Resolved", "Ambient", "Normal", "Light", "Texture coordinates",
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
    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::vector<std::shared_ptr<Shader>> mpShaders;
    std::shared_ptr<Deferred> mpRenderPass;

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
