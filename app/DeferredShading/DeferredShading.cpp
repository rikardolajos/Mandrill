#include "Mandrill.h"

using namespace Mandrill;

class DeferredShading : public App
{
public:
    enum {
        GBUFFER_PASS = 0,
        RESOLVE_PASS = 1,
    };

    struct PushConstants {
        int renderMode;
        float time;
    };

    static std::shared_ptr<Image> createColorAttachmentImage(std::shared_ptr<Device> pDevice, uint32_t width,
                                                             uint32_t height, VkFormat format)
    {
        return pDevice->createImage(width, height, 1, 1, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL,
                                    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    void transitionAttachmentsForGBuffer(VkCommandBuffer cmd)
    {
        for (auto& attachment : mColorAttachments) {
            Helpers::imageBarrier(cmd, attachment->getImage(), VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_IMAGE_LAYOUT_GENERAL);
        }
    }

    void transitionAttachmentsForResolve(VkCommandBuffer cmd)
    {
        for (auto& attachment : mColorAttachments) {
            Helpers::imageBarrier(cmd, attachment->getImage(), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
        }
    }

    void createAttachments()
    {
        uint32_t width = mpSwapchain->getExtent().width;
        uint32_t height = mpSwapchain->getExtent().height;
        VkFormat depthFormat = Helpers::findDepthFormat(mpDevice);

        mColorAttachments.clear();
        mColorAttachments.push_back(createColorAttachmentImage(mpDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT));
        mColorAttachments.push_back(createColorAttachmentImage(mpDevice, width, height, VK_FORMAT_R16G16B16A16_SFLOAT));
        mColorAttachments.push_back(createColorAttachmentImage(mpDevice, width, height, VK_FORMAT_R8G8B8A8_UNORM));
        mpDepthAttachment =
            mpDevice->createImage(width, height, 1, 1, VK_SAMPLE_COUNT_1_BIT, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // Create image views and transition image layouts
        VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);

        for (auto& attachment : mColorAttachments) {
            attachment->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);

            // Transition to correct layout for descriptor creation
            Helpers::imageBarrier(cmd, attachment->getImage(), VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
                                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        }

        // Transition depth attachment for depth use
        VkImageSubresourceRange depthSubresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                                         .baseMipLevel = 0,
                                                         .levelCount = 1,
                                                         .baseArrayLayer = 0,
                                                         .layerCount = 1};
        if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
            depthSubresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        Helpers::imageBarrier(
            cmd, mpDepthAttachment->getImage(), VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, &depthSubresourceRange);

        Helpers::cmdEnd(mpDevice, cmd);
    }

    void createAttachmentDescriptor()
    {
        std::vector<DescriptorDesc> descriptorDesc;
        for (auto& attachment : mColorAttachments) {
            descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, attachment);
            descriptorDesc.back().imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        mpColorAttachmentDescriptor = mpDevice->createDescriptor(
            descriptorDesc, mPipelines[RESOLVE_PASS]->getShader()->getDescriptorSetLayout(0));
    }

    DeferredShading() : App("Deferred Shading", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight (default
        mpSwapchain = mpDevice->createSwapchain();

        // Create scene
        mpScene = mpDevice->createScene();

        // Create the attachments
        createAttachments();

        // Create two passes
        mpGBufferPass = mpDevice->createPass(mColorAttachments, mpDepthAttachment);
        mpResolvePass = mpDevice->createPass(mpSwapchain->getExtent(), mpSwapchain->getImageFormat());

        // Create two shaders (and pipelines) for G-buffer and resolve pass respectively
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("DeferredShading/GBuffer.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("DeferredShading/GBuffer.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pGBufferShader = mpDevice->createShader(shaderDesc);

        shaderDesc.clear();
        shaderDesc.emplace_back("DeferredShading/Resolve.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("DeferredShading/Resolve.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pResolveShader = mpDevice->createShader(shaderDesc);

        // Create pipelines
        PipelineDesc pipelineDesc;
        pipelineDesc.depthTestEnable = VK_TRUE;
        mPipelines.emplace_back(mpDevice->createPipeline(mpGBufferPass, pGBufferShader, pipelineDesc));

        pipelineDesc.depthTestEnable = VK_FALSE;
        mPipelines.emplace_back(mpDevice->createPipeline(mpResolvePass, pResolveShader, pipelineDesc));

        // Create descriptor for resolve pass input attachments
        createAttachmentDescriptor();

        // Load scene
        auto meshIndices = mpScene->addMeshFromFile(GetResourcePath("scenes/crytek_sponza/sponza.obj"));
        uint32_t nodeIndex = mpScene->addNode();
        mpScene->getNodes()[nodeIndex].setPipeline(mPipelines[GBUFFER_PASS]); // Render scene with first pass pipeline
        for (auto meshIndex : meshIndices) {
            mpScene->getNodes()[nodeIndex].addMesh(meshIndex);
        }
        // Scale down the model
        mpScene->getNodes()[nodeIndex].setTransform(glm::scale(glm::vec3(0.01f)));

        mpScene->compile(mpSwapchain->getFramesInFlightCount());
        mpScene->createDescriptors(mPipelines[GBUFFER_PASS]->getShader()->getDescriptorSetLayouts(),
                                   mpSwapchain->getFramesInFlightCount());
        mpScene->syncToDevice();

        // Activate back-face culling for G-buffer pass
        mPipelines[GBUFFER_PASS]->setCullMode(VK_CULL_MODE_BACK_BIT);

        // Setup camera
        mpCamera = mpDevice->createCamera(mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);
        mpCamera->createDescriptor(VK_SHADER_STAGE_VERTEX_BIT);

        // Initialize GUI
        App::createGUI(mpDevice, mpResolvePass);
    }

    ~DeferredShading()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta)
    {
        mpSwapchain->waitForFence();

        if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
            mpCamera->update(delta, getCursorDelta());
        }
    }

    void render() override
    {
        // Check if camera matrix and attachments need to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->updateAspectRatio();
            createAttachments();
            createAttachmentDescriptor();
            mpGBufferPass->update(mColorAttachments, mpDepthAttachment);
            mpResolvePass->update(mpSwapchain->getExtent());
        }

        // Acquire frame from swapchain and prepare rasterizer
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();

        // Transition color attachments to correct image layout
        transitionAttachmentsForGBuffer(cmd);

        // Begin G-Buffer pass
        mpGBufferPass->begin(cmd, glm::vec4(0.2f, 0.6f, 1.0f, 1.0f));

        // Render scene
        mpScene->render(cmd, mpCamera, mpSwapchain->getInFlightIndex());

        // End the G-Buffer pass without any implicit image transitions
        vkCmdEndRendering(cmd);

        // Transition color attachments to correct image layout
        transitionAttachmentsForResolve(cmd);

        mpResolvePass->begin(cmd);

        // Bind the pipeline for the full-screen quad
        mPipelines[RESOLVE_PASS]->bind(cmd);

        // Bind descriptors for color attachments
        mpColorAttachmentDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines[RESOLVE_PASS]->getLayout(),
                                          0);

        // Push constants
        PushConstants pushConstants = {
            .renderMode = mRenderMode,
            .time = mTime,
        };
        vkCmdPushConstants(cmd, mPipelines[RESOLVE_PASS]->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstants), &pushConstants);

        // Render full-screen quad to resolve final composition
        vkCmdDraw(cmd, 3, 1, 0, 0);

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpResolvePass->end(cmd);
        mpSwapchain->present(cmd, mpResolvePass->getOutput());
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        // Render the base GUI, the menu bar with it's subwindows
        App::baseGUI(mpDevice, mpSwapchain, mPipelines);

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
        App::baseKeyCallback(window, key, scancode, action, mods, mpDevice, mpSwapchain, mPipelines);
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
    std::shared_ptr<Pass> mpGBufferPass;
    std::shared_ptr<Pass> mpResolvePass;
    std::vector<std::shared_ptr<Pipeline>> mPipelines;

    std::vector<std::shared_ptr<Image>> mColorAttachments;
    std::shared_ptr<Descriptor> mpColorAttachmentDescriptor;

    std::shared_ptr<Image> mpDepthAttachment;

    std::shared_ptr<Scene> mpScene;
    std::shared_ptr<Camera> mpCamera;

    int mRenderMode = 0;
};

int main()
{
    DeferredShading app = DeferredShading();
    app.run();
    return 0;
}
