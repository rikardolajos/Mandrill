#include "Mandrill.h"

using namespace Mandrill;

class RayTracerApp : public App
{
public:
    VkWriteDescriptorSet getRayTracingImageDescriptor(uint32_t binding)
    {
        static VkDescriptorImageInfo ii;
        ii.imageView = mpSwapchain->getImageView();
        ii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet descriptor = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &ii,
        };

        return descriptor;
    }

    RayTracerApp() : App("Ray Tracer App", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a render pass (only needed for ImGUI)
        mpRenderPass = std::make_shared<RayTracing>(mpDevice, mpSwapchain);

        // Create a scene so we can access the layout, the actual scene will be loaded later
        mpScene = std::make_shared<Scene>(mpDevice, mpSwapchain, true);

        //std::vector<LayoutDesc> layoutDesc;
        //layoutDesc.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        //layoutDesc.emplace_back(0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        //layoutDesc.emplace_back(0, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        //layoutDesc.emplace_back(0, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        //layoutDesc.emplace_back(0, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        //auto pLayout =
        //    std::make_shared<Layout>(mpDevice, layoutDesc, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

        // Create a sampler that will be used to render materials
        mpSampler = std::make_shared<Sampler>(mpDevice);

        // Load scene
        auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\crytek_sponza\\sponza.obj");
        // auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\viking_room\\viking_room.obj");
        // auto meshIndices2 = mpScene->addMeshFromFile("D:\\scenes\\box\\box.obj");
        std::shared_ptr<Node> pNode = mpScene->addNode();
        for (auto meshIndex : meshIndices) {
            pNode->addMesh(meshIndex);
        }

        // Scale down the model
        pNode->setTransform(glm::scale(glm::vec3(0.01f)));

        mpScene->setSampler(mpSampler);
        mpScene->compile();
        mpScene->syncToDevice();

        // Setup specialization constants with scene information for ray gen shader
        mSpecializationConstants.push_back(mpScene->getVertexCount());   // VERTEX_COUNT
        mSpecializationConstants.push_back(mpScene->getIndexCount());    // INDEX_COUNT
        mSpecializationConstants.push_back(mpScene->getMaterialCount()); // MATERIAL_COUNT
        mSpecializationConstants.push_back(mpScene->getTextureCount());  // TEXTURE_COUNT

        for (uint32_t i = 0; i < mSpecializationConstants.size(); i++) {
            VkSpecializationMapEntry entry = {
                .constantID = i,
                .offset = i * sizeof(uint32_t),
                .size = sizeof(uint32_t),
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
        shaderDesc.emplace_back("RayTracerApp/RayGen.rgen", "main", VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                &mSpecializationInfo);
        shaderDesc.emplace_back("RayTracerApp/RayMiss.rmiss", "main", VK_SHADER_STAGE_MISS_BIT_KHR);
        shaderDesc.emplace_back("RayTracerApp/RayClosestHit.rchit", "main", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        auto pShader = std::make_shared<Shader>(mpDevice, shaderDesc);

        // Create pipeline description with recursion depth and shader groups
        RayTracingPipelineDesc pipelineDesc(1, 1, 1);
        pipelineDesc.setRayGen(0);
        pipelineDesc.setMissGroup(0, 1);
        pipelineDesc.setHitGroup(0, 2);

        // Create ray-tracing pipeline
        auto pLayout = mpScene->getLayout();
        mpPipeline = std::make_shared<RayTracingPipeline>(mpDevice, pShader, pLayout, pipelineDesc);

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Initialize GUI
        App::createGUI(mpDevice, mpRenderPass->getRenderPass(), VK_SAMPLE_COUNT_1_BIT);
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

        // Push descriptor with image
        mpScene->bindRayTracingDescriptors(cmd, mpCamera, mpPipeline->getLayout());
        std::vector<VkWriteDescriptorSet> writes = {
            getRayTracingImageDescriptor(0),
        };
        vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mpPipeline->getLayout(), 4,
                                  static_cast<uint32_t>(writes.size()), writes.data());

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
        mpRenderPass->frameBegin(cmd, glm::vec4(0.0f, 0.4f, 0.2f, 1.0f));

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpRenderPass->frameEnd(cmd);
        mpSwapchain->present(cmd);
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

    std::vector<uint32_t> mSpecializationConstants;
    std::vector<VkSpecializationMapEntry> mSpecializationMapEntries;
    VkSpecializationInfo mSpecializationInfo;
};

int main()
{
    RayTracerApp app = RayTracerApp();
    app.run();
    return 0;
}
