#include "Mandrill.h"

using namespace Mandrill;

class RayTracerApp : public App
{
public:
    RayTracerApp() : App("Ray Tracer App", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a render pass (rasterizer, only needed for ImGUI)
        mpRenderPass = std::make_shared<Rasterizer>(mpDevice, mpSwapchain);

        // Create a scene so we can access the layout, the actual scene will be loaded later
        mpScene = std::make_shared<Scene>(mpDevice, mpSwapchain, true);
        auto pLayout = mpScene->getLayout();

        // Create a shader module with ray-tracing stages
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("RayTracerApp/RayGen.rgen", "main", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        shaderDesc.emplace_back("RayTracerApp/RayMiss.rmiss", "main", VK_SHADER_STAGE_MISS_BIT_KHR);
        shaderDesc.emplace_back("RayTracerApp/RayClosestHit.rchit", "main", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        auto pShader = std::make_shared<Shader>(mpDevice, shaderDesc);

        // Create pipeline description with recursion depth and shader groups
        RayTracingPipelineDesc pipelineDesc(1, 1, 1);
        pipelineDesc.setRayGen(0);
        pipelineDesc.setMissGroup(0, 1);
        pipelineDesc.setHitGroup(0, 2);

        // Create ray-tracing pipeline
        mpPipeline = std::make_shared<RayTracingPipeline>(mpDevice, pShader, pLayout, pipelineDesc);

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

    ~RayTracerApp()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta)
    {
        if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
            mpCamera->update(delta, getCursorDelta());
        }

        // Rebuild acceleration structure
        mpScene->buildAccelerationStructure();
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

        // Bind pipeline
        mpPipeline->bind(cmd);

        // Prepare image for writing
        mpPipeline->write(cmd, mpSwapchain->getImage());

        // Trace rays
        auto rayGenSBT = mpPipeline->getRayGenSBT();
        auto missSBT = mpPipeline->getMissSBT();
        auto hitSBT = mpPipeline->getHitSBT();
        auto callSBT = mpPipeline->getCallSBT();
        vkCmdTraceRaysKHR(cmd, &rayGenSBT, &missSBT, &hitSBT, &callSBT, mpSwapchain->getExtent().width,
                          mpSwapchain->getExtent().height, 1);

        // Draw GUI
        App::renderGUI(cmd);

        // Prepare image for presenting
        mpPipeline->present(cmd, mpSwapchain->getImage());

        // Submit command buffer to rasterizer and present swapchain frame
        mpRenderPass->frameEnd(cmd);
        mpSwapchain->present();
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        // Render the base GUI, the menu bar with it's subwindows
        App::baseGUI(mpDevice, mpSwapchain, nullptr, mpPipeline);

        // App-specific GUI elements
        if (ImGui::Begin("Ray Tracer App")) {
        }

        ImGui::End();
    }

    void appKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        // Invoke the base application's keyboard commands
        App::baseKeyCallback(window, key, scancode, action, mods, mpDevice, mpSwapchain, nullptr, mpPipeline);
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
    std::shared_ptr<RenderPass> mpRenderPass;
    std::shared_ptr<RayTracingPipeline> mpPipeline;

    std::shared_ptr<Scene> mpScene;
    std::shared_ptr<Camera> mpCamera;

    std::shared_ptr<Sampler> mpSampler;
    std::shared_ptr<Texture> mpTexture;
};

int main()
{
    RayTracerApp app = RayTracerApp();
    app.run();
    return 0;
}
