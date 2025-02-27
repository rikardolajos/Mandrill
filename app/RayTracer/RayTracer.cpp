#include "Mandrill.h"

using namespace Mandrill;

class RayTracer : public App
{
public:
    struct PushConstants {
        int renderMode;
        float time;
    };

    static std::shared_ptr<Image> createImage(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain)
    {
        auto image = std::make_shared<Image>(
            pDevice, pSwapchain->getExtent().width, pSwapchain->getExtent().height, 1, VK_SAMPLE_COUNT_1_BIT,
            pSwapchain->getImageFormat(), VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        image->createImageView(VK_IMAGE_ASPECT_COLOR_BIT);
        return image;
    }

    RayTracer() : App("Ray Tracer", 1920, 1080)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a pass for rendering GUI (depth attachment is not needed)
        mpPass = std::make_shared<Pass>(mpDevice, mpSwapchain, false);

        // Create an image to render to
        mpImage = createImage(mpDevice, mpSwapchain);

        // Create a scene (with ray-tracing support)
        mpScene = std::make_shared<Scene>(mpDevice, mpSwapchain, true);

        // Create a sampler that will be used to render materials
        mpSampler = std::make_shared<Sampler>(mpDevice);

        // Load scene
        // auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\crytek_sponza\\sponza.obj");
        // auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\viking_room\\viking_room.obj");
        // auto meshIndices = mpScene->addMeshFromFile("D:\\scenes\\pbr_box\\pbr_box.obj");
        auto meshIndices2 = mpScene->addMeshFromFile("D:\\scenes\\pbr_box\\pbr_box.obj");
        std::shared_ptr<Node> pNode = mpScene->addNode();
        // for (auto meshIndex : meshIndices) {
        //     pNode->addMesh(meshIndex);
        // }

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

        mpPipeline = std::make_shared<RayTracingPipeline>(mpDevice, pLayout, pShader, pipelineDesc);

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow, mpSwapchain);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Image descriptor (layout is in set 4 from scene)
        std::vector<DescriptorDesc> descriptorDesc;
        descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, mpImage);
        descriptorDesc.back().imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        mpImageDescriptor = std::make_shared<Descriptor>(mpDevice, descriptorDesc, pLayout->getDescriptorSetLayouts()[4]);

        // Initialize GUI
        App::createGUI(mpDevice, mpPass);
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
            // Also update render image since swapchain changed
            mpImage = createImage(mpDevice, mpSwapchain);
        }

        // Bind pipeline
        mpPipeline->bind(cmd);

        // Prepare image for writing
        mpPipeline->write(cmd, mpImage->getImage());

        // Push constants
        PushConstants pushConstants = {
            .renderMode = mRenderMode,
            .time = mTime,
        };
        vkCmdPushConstants(cmd, mpPipeline->getLayout(), VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof pushConstants,
                           &pushConstants);

        // Bind descriptors
        mpScene->bindRayTracingDescriptors(cmd, mpCamera, mpPipeline->getLayout());
        mpImageDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mpPipeline->getLayout(), 4);

        // Trace rays
        auto rayGenSBT = mpPipeline->getRayGenSBT();
        auto missSBT = mpPipeline->getMissSBT();
        auto hitSBT = mpPipeline->getHitSBT();
        auto callSBT = mpPipeline->getCallSBT();
        vkCmdTraceRaysKHR(cmd, &rayGenSBT, &missSBT, &hitSBT, &callSBT, mpSwapchain->getExtent().width,
                          mpSwapchain->getExtent().height, 1);

        // Prepare image for reading
        mpPipeline->read(cmd, mpImage->getImage());

        // Start pass
        mpPass->begin(cmd, mpImage);

        // Draw GUI
        App::renderGUI(cmd);

        // End pass
        mpPass->end(cmd, mpImage);

        // Submit command buffer and present
        mpSwapchain->present(cmd, mpImage);
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        App::baseGUI(mpDevice, mpSwapchain, mpPipeline);

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
        App::baseKeyCallback(window, key, scancode, action, mods, mpDevice, mpSwapchain, mpPipeline);
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
    std::shared_ptr<Pass> mpPass;
    std::shared_ptr<RayTracingPipeline> mpPipeline;
    std::shared_ptr<Image> mpImage;
    std::shared_ptr<Descriptor> mpImageDescriptor;

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
