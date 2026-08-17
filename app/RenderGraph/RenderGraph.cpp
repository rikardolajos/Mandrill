#include "Mandrill.h"

using namespace Mandrill;

// A deferred renderer with a bloom chain, put together as a render graph. The graph owns every image the frame is
// built from and works out the order of the passes, the layout transitions and the barriers between them, so the app
// only says which resources each pass reads and writes and what to draw.
class RenderGraphApp : public App
{
public:
    enum RenderMode {
        RENDER_MODE_FINAL = 0,
        RENDER_MODE_HDR,
        RENDER_MODE_BLOOM,
        RENDER_MODE_POSITION,
        RENDER_MODE_NORMAL,
        RENDER_MODE_ALBEDO,
    };

    struct LightingPushConstants {
        float time;
        float lightIntensity;
    };

    struct BlurPushConstants {
        int horizontal;
        float threshold;
    };

    struct CompositePushConstants {
        float exposure;
        float bloomStrength;
        int renderMode;
    };

    RenderGraphApp() : App("Render Graph", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = make_ptr<Device>(mpWindow);

        // Create a swapchain
        mpSwapchain = mpDevice->createSwapchain();

        // Create scene
        mpScene = mpDevice->createScene();

        // Describe every image the frame is built from
        mpGraph = mpDevice->createRenderGraph();
        addGraphResources();

        // Add the passes in an order that is not the order they run in, since it is the resources a pass reads and
        // writes that decide where it ends up, not when it was added
        mpGraph->addPass("Composite", {"hdr", "bloom", "position", "normal", "albedo"}, {"output"},
                         [this](VkCommandBuffer cmd) { compositePass(cmd); });
        mpGraph->addPass("Blur vertical", {"bloomHorizontal"}, {"bloom"},
                         [this](VkCommandBuffer cmd) { blurPass(cmd, BLUR_VERTICAL_PASS); });
        mpGraph->addPass("G-buffer", {}, {"position", "normal", "albedo", "depth"},
                         [this](VkCommandBuffer cmd) { gBufferPass(cmd); });
        mpGraph->addPass("Blur horizontal", {"hdr"}, {"bloomHorizontal"},
                         [this](VkCommandBuffer cmd) { blurPass(cmd, BLUR_HORIZONTAL_PASS); });
        mpGraph->addPass("Lighting", {"position", "normal", "albedo"}, {"hdr"},
                         [this](VkCommandBuffer cmd) { lightingPass(cmd); });

        mpGraph->compile();

        // Create the passes that render into the graph's resources
        createPasses();

        // Create the shaders, one per pass, where the two blurs share the source but not the resources they read
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("RenderGraph/GBuffer.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("RenderGraph/GBuffer.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pGBufferShader = mpDevice->createShader(shaderDesc);

        auto createFullscreenShader = [this](const std::string& fragmentShader) {
            std::vector<ShaderDesc> desc;
            desc.emplace_back("RenderGraph/Fullscreen.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
            desc.emplace_back(fragmentShader, "main", VK_SHADER_STAGE_FRAGMENT_BIT);
            return mpDevice->createShader(desc);
        };

        auto pLightingShader = createFullscreenShader("RenderGraph/Lighting.frag");
        auto pBlurHorizontalShader = createFullscreenShader("RenderGraph/Blur.frag");
        auto pBlurVerticalShader = createFullscreenShader("RenderGraph/Blur.frag");
        auto pCompositeShader = createFullscreenShader("RenderGraph/Composite.frag");

        // Create the pipelines, in the order of the pass enum
        PipelineDesc pipelineDesc;
        pipelineDesc.depthTestEnable = VK_TRUE;
        mPipelines.emplace_back(mpDevice->createPipeline(mpGBufferPass, pGBufferShader, pipelineDesc));

        pipelineDesc.depthTestEnable = VK_FALSE;
        mPipelines.emplace_back(mpDevice->createPipeline(mpLightingPass, pLightingShader, pipelineDesc));
        mPipelines.emplace_back(mpDevice->createPipeline(mpBlurHorizontalPass, pBlurHorizontalShader, pipelineDesc));
        mPipelines.emplace_back(mpDevice->createPipeline(mpBlurVerticalPass, pBlurVerticalShader, pipelineDesc));
        mPipelines.emplace_back(mpDevice->createPipeline(mpCompositePass, pCompositeShader, pipelineDesc));

        // Hand the graph's images to the shaders that read them
        setShaderResources();

        // Load scene
        auto meshIndices = mpScene->addMeshFromFile(GetResourcePath("scenes/crytek_sponza/sponza.obj"));
        uint32_t nodeIndex = mpScene->addNode();
        mpScene->getNodes()[nodeIndex].setPipeline(mPipelines[GBUFFER_PASS]);
        for (auto meshIndex : meshIndices) {
            mpScene->getNodes()[nodeIndex].addMesh(meshIndex);
        }
        // Scale down the model
        mpScene->getNodes()[nodeIndex].setTransform(glm::scale(glm::vec3(0.01f)));

        mpScene->compile();

        // Activate back-face culling for the G-buffer pass
        mPipelines[GBUFFER_PASS]->setCullMode(VK_CULL_MODE_BACK_BIT);

        // Setup camera
        mpCamera = mpDevice->createCamera();
        mpCamera->setPosition(glm::vec3(5.0f, 1.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 1.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Create descriptors for scene and camera, and sync to device
        mpScene->createDescriptors(mpCamera);
        mpScene->syncToDevice();

        // Initialize GUI, which is drawn by the pass that produces the final image
        App::createGUI(mpDevice, mpCompositePass);
    }

    ~RenderGraphApp()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta) override
    {
        mpSwapchain->waitForFence();

        if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
            mpCamera->update(mpWindow, delta, getCursorDelta());
        }
    }

    void render() override
    {
        // Check if camera and graph resources need to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->setAspectRatio(mpSwapchain->getAspectRatio());

            // Adding the resources again at the new extent and compiling again recreates them
            addGraphResources();
            mpGraph->compile();

            updatePasses();
            setShaderResources();
        }

        // Acquire frame from swapchain
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();

        // Record every pass of the graph, in the order the graph resolved
        mpGraph->execute(cmd);

        // The output resource was declared with a final layout of transfer source, so it is ready to be presented
        mpSwapchain->present(cmd, mpGraph->getResource("output"));
    }

    void appGUI(ImGuiContext* pContext) override
    {
        ImGui::SetCurrentContext(pContext);

        // Render the base GUI, the menu bar with it's subwindows
        App::baseGUI(mpDevice, mpSwapchain, mPipelines);

        ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Render Graph")) {
            const char* renderModes[] = {
                "Final", "HDR", "Bloom", "Position", "Normal", "Albedo",
            };
            ImGui::Combo("Render mode", &mRenderMode, renderModes, IM_ARRAYSIZE(renderModes));

            ImGui::SeparatorText("Lighting");
            ImGui::SliderFloat("Intensity", &mLightIntensity, 0.0f, 20.0f);
            ImGui::SliderFloat("Exposure", &mExposure, 0.1f, 5.0f);

            ImGui::SeparatorText("Bloom");
            ImGui::SliderFloat("Threshold", &mBloomThreshold, 0.0f, 4.0f);
            ImGui::SliderFloat("Strength", &mBloomStrength, 0.0f, 2.0f);

            ImGui::SeparatorText("Execution order");
            uint32_t index = 1;
            for (const auto& pass : mpGraph->getExecutionOrder()) {
                ImGui::Text("%u. %s", index++, pass.c_str());
            }
        }

        ImGui::End();
    }

    void appKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods) override
    {
        App::baseKeyCallback(pWindow, key, scancode, action, mods, mpDevice, mpSwapchain, mPipelines);
    }

    void appCursorPosCallback(GLFWwindow* pWindow, double xPos, double yPos) override
    {
        App::baseCursorPosCallback(pWindow, xPos, yPos);
    }

    void appMouseButtonCallback(GLFWwindow* pWindow, int button, int action, int mods) override
    {
        App::baseMouseButtonCallback(pWindow, button, action, mods, mpCamera);
    }

private:
    enum {
        GBUFFER_PASS = 0,
        LIGHTING_PASS,
        BLUR_HORIZONTAL_PASS,
        BLUR_VERTICAL_PASS,
        COMPOSITE_PASS,
    };

    void addGraphResources()
    {
        VkExtent2D extent = mpSwapchain->getExtent();

        // The G-buffer attachments are written as color attachments and read back as storage images, so they are
        // declared with both usages and the graph transitions them accordingly
        const VkImageUsageFlags attachmentAndStorage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

        mpGraph->addResource("position", VK_FORMAT_R16G16B16A16_SFLOAT, extent, attachmentAndStorage);
        mpGraph->addResource("normal", VK_FORMAT_R16G16B16A16_SFLOAT, extent, attachmentAndStorage);
        mpGraph->addResource("albedo", VK_FORMAT_R8G8B8A8_UNORM, extent, attachmentAndStorage);
        mpGraph->addResource("depth", Helpers::findDepthFormat(mpDevice), extent,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

        mpGraph->addResource("hdr", VK_FORMAT_R16G16B16A16_SFLOAT, extent, attachmentAndStorage);
        mpGraph->addResource("bloomHorizontal", VK_FORMAT_R16G16B16A16_SFLOAT, extent, attachmentAndStorage);
        mpGraph->addResource("bloom", VK_FORMAT_R16G16B16A16_SFLOAT, extent, attachmentAndStorage);

        // The final image leaves the graph as a blit into the swapchain image
        mpGraph->addResource("output", mpSwapchain->getImageFormat(), extent,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }

    std::vector<ptr<Image>> gBufferAttachments() const
    {
        return {mpGraph->getResource("position"), mpGraph->getResource("normal"), mpGraph->getResource("albedo")};
    }

    void createPasses()
    {
        mpGBufferPass = mpDevice->createPass(gBufferAttachments(), mpGraph->getResource("depth"));
        mpLightingPass = mpDevice->createPass({mpGraph->getResource("hdr")}, nullptr);
        mpBlurHorizontalPass = mpDevice->createPass({mpGraph->getResource("bloomHorizontal")}, nullptr);
        mpBlurVerticalPass = mpDevice->createPass({mpGraph->getResource("bloom")}, nullptr);
        mpCompositePass = mpDevice->createPass({mpGraph->getResource("output")}, nullptr);
    }

    void updatePasses()
    {
        mpGBufferPass->update(gBufferAttachments(), mpGraph->getResource("depth"));
        mpLightingPass->update({mpGraph->getResource("hdr")}, nullptr);
        mpBlurHorizontalPass->update({mpGraph->getResource("bloomHorizontal")}, nullptr);
        mpBlurVerticalPass->update({mpGraph->getResource("bloom")}, nullptr);
        mpCompositePass->update({mpGraph->getResource("output")}, nullptr);
    }

    // Attach the graph's images to the shaders that read them, by the name they have in the shader
    void setShaderResources()
    {
        auto pLightingShader = mPipelines[LIGHTING_PASS]->getShader();
        pLightingShader->setResource("inPosition", mpGraph->getResource("position"));
        pLightingShader->setResource("inNormal", mpGraph->getResource("normal"));
        pLightingShader->setResource("inAlbedo", mpGraph->getResource("albedo"));

        mPipelines[BLUR_HORIZONTAL_PASS]->getShader()->setResource("inImage", mpGraph->getResource("hdr"));
        mPipelines[BLUR_VERTICAL_PASS]->getShader()->setResource("inImage", mpGraph->getResource("bloomHorizontal"));

        auto pCompositeShader = mPipelines[COMPOSITE_PASS]->getShader();
        pCompositeShader->setResource("inHdr", mpGraph->getResource("hdr"));
        pCompositeShader->setResource("inBloom", mpGraph->getResource("bloom"));
        pCompositeShader->setResource("inPosition", mpGraph->getResource("position"));
        pCompositeShader->setResource("inNormal", mpGraph->getResource("normal"));
        pCompositeShader->setResource("inAlbedo", mpGraph->getResource("albedo"));
    }

    // Draw a full-screen triangle with the pipeline of a pass
    void drawFullscreen(VkCommandBuffer cmd, uint32_t pipelineIndex, const void* pPushConstants, uint32_t size)
    {
        mPipelines[pipelineIndex]->bind(cmd);
        mPipelines[pipelineIndex]->getShader()->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 0);
        vkCmdPushConstants(cmd, mPipelines[pipelineIndex]->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, size,
                           pPushConstants);
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    void gBufferPass(VkCommandBuffer cmd)
    {
        mpGBufferPass->begin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        mpScene->render(cmd, mpCamera);
        mpGBufferPass->end(cmd);
    }

    void lightingPass(VkCommandBuffer cmd)
    {
        LightingPushConstants pushConstants = {
            .time = mTime,
            .lightIntensity = mLightIntensity,
        };

        mpLightingPass->begin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        drawFullscreen(cmd, LIGHTING_PASS, &pushConstants, sizeof(pushConstants));
        mpLightingPass->end(cmd);
    }

    void blurPass(VkCommandBuffer cmd, uint32_t pipelineIndex)
    {
        bool horizontal = pipelineIndex == BLUR_HORIZONTAL_PASS;

        // The horizontal blur is the one that extracts the bright parts, the vertical one blurs what it produced
        BlurPushConstants pushConstants = {
            .horizontal = horizontal ? 1 : 0,
            .threshold = horizontal ? mBloomThreshold : 0.0f,
        };

        auto pPass = horizontal ? mpBlurHorizontalPass : mpBlurVerticalPass;

        pPass->begin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        drawFullscreen(cmd, pipelineIndex, &pushConstants, sizeof(pushConstants));
        pPass->end(cmd);
    }

    void compositePass(VkCommandBuffer cmd)
    {
        CompositePushConstants pushConstants = {
            .exposure = mExposure,
            .bloomStrength = mBloomStrength,
            .renderMode = mRenderMode,
        };

        mpCompositePass->begin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        drawFullscreen(cmd, COMPOSITE_PASS, &pushConstants, sizeof(pushConstants));

        // Draw GUI on top of the final image
        App::renderGUI(cmd);

        mpCompositePass->end(cmd);
    }

    ptr<Device> mpDevice;
    ptr<Swapchain> mpSwapchain;

    ptr<RenderGraph> mpGraph;

    ptr<Pass> mpGBufferPass;
    ptr<Pass> mpLightingPass;
    ptr<Pass> mpBlurHorizontalPass;
    ptr<Pass> mpBlurVerticalPass;
    ptr<Pass> mpCompositePass;

    std::vector<ptr<Pipeline>> mPipelines;

    ptr<Scene> mpScene;
    ptr<Camera> mpCamera;

    int mRenderMode = RENDER_MODE_FINAL;
    float mLightIntensity = 8.0f;
    float mBloomThreshold = 1.0f;
    float mBloomStrength = 0.6f;
    float mExposure = 1.0f;
};

int main()
{
    RenderGraphApp app = RenderGraphApp();
    app.run();
    return 0;
}
