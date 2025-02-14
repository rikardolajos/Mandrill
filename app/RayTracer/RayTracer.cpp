#include "Mandrill.h"

using namespace Mandrill;

class RayTracer : public App
{
public:
    struct PushConstants {
        int renderMode;
        float time;
    };

    RayTracer() : App("Ray Tracer", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a render pass for ImGUI
        mpRenderPass = std::make_shared<GUI>(mpDevice, mpSwapchain);

        // Create a scene so we can access the layout, the actual scene will be loaded later
        mpScene = std::make_shared<Scene>(mpDevice, mpSwapchain, true);

        // Create a sampler that will be used to render materials
        mpSampler = std::make_shared<Sampler>(mpDevice);

        // Load scene
        auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\crytek_sponza\\sponza.obj");
        // auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\viking_room\\viking_room.obj");
        // auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\pbr_box\\pbr_box.obj");
        auto meshIndices2 = mpScene->addMeshFromFile("D:\\scenes\\pbr_box\\pbr_box.obj");
        std::shared_ptr<Node> pNode = mpScene->addNode();
        for (auto meshIndex : meshIndices) {
            pNode->addMesh(meshIndex);
        }

        // Scale down the model
        pNode->setTransform(glm::scale(glm::vec3(0.01f)));

        mpCube = mpScene->addNode();
        for (auto meshIndex : meshIndices2) {
            mpCube->addMesh(meshIndex);
        }

        mpScene->setSampler(mpSampler);
        mpScene->compile();
        mpScene->syncToDevice();

        // Setup specialization constants with scene information for ray gen shader
        mSpecializationConstants.push_back(mpScene->getVertexCount());   // VERTEX_COUNT
        mSpecializationConstants.push_back(mpScene->getIndexCount());    // INDEX_COUNT
        mSpecializationConstants.push_back(mpScene->getMaterialCount()); // MATERIAL_COUNT
        mSpecializationConstants.push_back(mpScene->getTextureCount());  // TEXTURE_COUNT
        mSpecializationConstants.push_back(mpScene->getMeshCount());     // MESH_COUNT

        for (uint32_t i = 0; i < mSpecializationConstants.size(); i++) {
            VkSpecializationMapEntry entry = {
                .constantID = i,
                .offset = i * static_cast<uint32_t>(sizeof(uint32_t)),
                .size = static_cast<uint32_t>(sizeof(uint32_t)),
            };
            mSpecializationMapEntries.push_back(entry);
        }

        mSpecializationInfo = {
            .mapEntryCount = static_cast<uint32_t>(mSpecializationMapEntries.size()),
            .pMapEntries = mSpecializationMapEntries.data(),
            .dataSize = mSpecializationConstants.size() * sizeof(uint32_t),
            .pData = mSpecializationConstants.data(),
        };

        // Create a shader module with ray-tracing stages
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("RayTracer/RayGen.rgen", "main", VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        shaderDesc.emplace_back("RayTracer/RayMiss.rmiss", "main", VK_SHADER_STAGE_MISS_BIT_KHR);
        shaderDesc.emplace_back("RayTracer/RayClosestHit.rchit", "main", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                &mSpecializationInfo);
        auto pShader = std::make_shared<Shader>(mpDevice, shaderDesc);

        // Create pipeline description with recursion depth and shader groups
        RayTracingPipelineDesc pipelineDesc(1, 1, 1);
        pipelineDesc.setRayGen(0);
        pipelineDesc.setMissGroup(0, 1);
        pipelineDesc.setHitGroup(0, 2);

        // Create ray-tracing pipeline
        auto pLayout = mpScene->getLayout();

        VkPushConstantRange pushConstantRange = {
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .offset = 0,
            .size = sizeof(PushConstants),
        };
        pLayout->addPushConstantRange(pushConstantRange);

        mpPipeline = std::make_shared<RayTracingPipeline>(mpDevice, pShader, pLayout, pipelineDesc);

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Initialize GUI
        App::createGUI(mpDevice, mpRenderPass->getRenderPass(), VK_SAMPLE_COUNT_1_BIT);
    }

    ~RayTracer()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta)
    {
        if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
            mpCamera->update(delta, getCursorDelta());
        }

        mTime += delta;

        mAngle += mRotationSpeed * delta;

        glm::mat4 transform = glm::scale(glm::vec3(0.5f));
        transform = glm::translate(transform, glm::vec3(0.0f, 5.0f, 0.0f));
        transform = glm::rotate(transform, mAngle, glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, 3.0f * mAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        mpCube->setTransform(transform);

        // Update acceleration structure
        mpScene->updateAccelerationStructure();
    }

    void render() override
    {
        // Acquire frame from swapchain
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();

        // Check if camera matrix needs to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->updateAspectRatio();
        }

        // Bind pipeline
        mpPipeline->bind(cmd);

        // Prepare image for writing
        mpPipeline->write(cmd, mpSwapchain->getImage());

        // Push constants
        PushConstants pushConstants = {
            .renderMode = mRenderMode,
            .time = mTime,
        };
        vkCmdPushConstants(cmd, mpPipeline->getLayout(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof pushConstants,
                           &pushConstants);

        // Bind descriptors
        mpScene->bindRayTracingDescriptors(cmd, mpCamera, mpPipeline->getLayout());
        auto imageDescriptorSet = mpSwapchain->getImageDescriptorSet();
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mpPipeline->getLayout(), 4, 1,
                                &imageDescriptorSet, 0, nullptr);

        // Trace rays
        auto rayGenSBT = mpPipeline->getRayGenSBT();
        auto missSBT = mpPipeline->getMissSBT();
        auto hitSBT = mpPipeline->getHitSBT();
        auto callSBT = mpPipeline->getCallSBT();
        vkCmdTraceRaysKHR(cmd, &rayGenSBT, &missSBT, &hitSBT, &callSBT, mpSwapchain->getExtent().width,
                          mpSwapchain->getExtent().height, 1);

        // Prepare image for reading
        mpPipeline->read(cmd, mpSwapchain->getImage());

        // Start rasterization render pass for ImGUI
        mpRenderPass->begin(cmd, glm::vec4(0.0f, 0.4f, 0.2f, 1.0f));

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpRenderPass->end(cmd);
        mpSwapchain->present(cmd);
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        // Render the base GUI, the menu bar with it's subwindows
        App::baseGUI(mpDevice, mpSwapchain, nullptr, mpPipeline);

        // App-specific GUI elements
        if (ImGui::Begin("Ray Tracer")) {
            const char* renderModes[] = {
                "Diffuse",
                "Normal",
            };
            ImGui::Combo("Render mode", &mRenderMode, renderModes, IM_ARRAYSIZE(renderModes));
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

    std::vector<uint32_t> mSpecializationConstants;
    std::vector<VkSpecializationMapEntry> mSpecializationMapEntries;
    VkSpecializationInfo mSpecializationInfo;

    int mRenderMode = 0;
    float mTime = 0.0f;

    std::shared_ptr<Node> mpCube;
    float mRotationSpeed = 0.2f;
    float mAngle = 0.0f;
};

int main()
{
    RayTracer app = RayTracer();
    app.run();
    return 0;
}
