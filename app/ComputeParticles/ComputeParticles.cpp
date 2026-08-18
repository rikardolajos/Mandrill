#include "Mandrill.h"

using namespace Mandrill;

class ComputeParticles : public App
{
public:
    static constexpr uint32_t kParticleCount = 256 * 1024;

    // Matches the Particle struct in the shaders
    struct Particle {
        glm::vec4 position;
        glm::vec4 velocity;
    };

    struct ComputePushConstant {
        glm::vec3 attractor;
        float strength;
        float delta;
        float damping;
        uint32_t particleCount;
    };

    struct RenderPushConstant {
        float pointSize;
        float speedScale;
    };

    void setupParticles()
    {
        std::vector<Particle> particles(kParticleCount);

        constexpr float twoPi = 6.283185307f;

        for (auto& particle : particles) {
            // Spread the particles over a thin disc and give them a tangential velocity, so that the simulation
            // starts out as a rotating cloud rather than a collapsing one
            float angle = twoPi * Helpers::random();
            float radius = 3.0f + 3.0f * Helpers::random();
            float height = 0.5f * (Helpers::random() - 0.5f);

            glm::vec3 position = glm::vec3(radius * std::cos(angle), height, radius * std::sin(angle));
            glm::vec3 velocity = 0.4f * glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), position);

            particle.position = glm::vec4(position, 0.0f);
            particle.velocity = glm::vec4(velocity, 0.0f);
        }

        mpParticleBuffer->copyFromHost(particles.data(), particles.size() * sizeof(Particle));
    }

    ComputeParticles() : App("Compute Particles", 1280, 720)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain
        mpSwapchain = mpDevice->createSwapchain();

        // Create a pass with 1 color attachment, depth attachment and multisampling
        mpPass = mpDevice->createPass(mpSwapchain->getExtent(), mpSwapchain->getImageFormat(), 1, true,
                                      mpDevice->getSampleCount());

        // Create the compute pipeline that advances the simulation
        std::vector<ShaderDesc> computeShaderDesc;
        computeShaderDesc.emplace_back("ComputeParticles/Particles.comp", "main", VK_SHADER_STAGE_COMPUTE_BIT);
        auto pComputeShader = mpDevice->createShader(computeShaderDesc);
        mpComputePipeline = mpDevice->createComputePipeline(pComputeShader, ComputePipelineDesc());

        // The particles are read from the storage buffer by vertex index, so there is no vertex input to describe
        std::vector<VkVertexInputBindingDescription> emptyBindingDescription;
        std::vector<VkVertexInputAttributeDescription> emptyAttributeDescription;

        // Blend the particles additively so that dense regions light up
        std::vector<VkPipelineColorBlendAttachmentState> blendStates = {{{
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        }}};

        PipelineDesc pipelineDesc(emptyBindingDescription, emptyAttributeDescription, blendStates);
        pipelineDesc.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        pipelineDesc.depthTestEnable = VK_FALSE;
        pipelineDesc.depthWriteEnable = VK_FALSE;

        // Create the graphics pipeline that draws the particles as point sprites
        std::vector<ShaderDesc> renderShaderDesc;
        renderShaderDesc.emplace_back("ComputeParticles/Particles.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        renderShaderDesc.emplace_back("ComputeParticles/Particles.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pRenderShader = mpDevice->createShader(renderShaderDesc);
        mpRenderPipeline = mpDevice->createPipeline(mpPass, pRenderShader, pipelineDesc);

        // Setup camera
        mpCamera = mpDevice->createCamera();
        mpCamera->setPosition(glm::vec3(0.0f, 6.0f, 14.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);
        mpCamera->setAspectRatio(mpSwapchain->getAspectRatio());

        // The particle state lives on the device: the compute shader writes it and the vertex shader reads it, so it
        // never has to be touched by the host again after it has been seeded
        mpParticleBuffer =
            mpDevice->createBuffer(kParticleCount * sizeof(Particle),
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        setupParticles();

        // Attach the resources to the names they have in the shaders. The buffer is declared without an instance
        // name in both shaders, so it is reached through its block type name.
        pComputeShader->setResource("Particles", mpParticleBuffer);
        pRenderShader->setResource("camera", mpCamera->getUniformBuffer());
        pRenderShader->setResource("Particles", mpParticleBuffer);

        // Initialize GUI
        App::createGUI(mpDevice, mpPass);
    }

    ~ComputeParticles()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta)
    {
        // Wait for GPU to finish rendering and using shared resources
        mpSwapchain->waitForFence();

        if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
            mpCamera->update(mpWindow, delta, getCursorDelta());
        }

        // A long frame would make the integration blow up, so the time step is capped
        mDelta = mPaused ? 0.0f : std::min(delta, 1.0f / 30.0f);
        mTime += mDelta;

        // Move the attractor along a slow figure-of-eight, so that the cloud keeps getting stirred
        mAttractor = glm::vec3(4.0f * std::sin(0.7f * mTime), 2.0f * std::sin(1.4f * mTime), 4.0f * std::cos(0.7f * mTime));
    }

    void render() override
    {
        // Check if camera matrix and attachments need to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->setAspectRatio(mpSwapchain->getAspectRatio());
            mpPass->update(mpSwapchain->getExtent());
        }

        // Acquire frame from swapchain
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();

        // A previous frame that is still in flight may be reading the particles, so the simulation has to wait for it
        // before overwriting them
        Helpers::bufferBarrier(cmd, mpParticleBuffer->getBuffer(), VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);

        // Advance the simulation. Dispatches cannot be recorded inside a pass, so this happens before it begins.
        mpComputePipeline->bind(cmd);

        ComputePushConstant computePushConstant = {
            .attractor = mAttractor,
            .strength = mStrength,
            .delta = mDelta,
            .damping = mDamping,
            .particleCount = kParticleCount,
        };
        vkCmdPushConstants(cmd, mpComputePipeline->getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof computePushConstant, &computePushConstant);

        mpComputePipeline->getShader()->bindResources(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

        // The dispatch is given in particles, the pipeline turns it into workgroups using the local size of the shader
        mpComputePipeline->dispatch(cmd, kParticleCount);

        // Let the vertex shader see what the compute shader just wrote
        Helpers::bufferBarrier(cmd, mpParticleBuffer->getBuffer(), VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT, VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                               VK_ACCESS_2_SHADER_STORAGE_READ_BIT);

        // Prepare rasterizer
        mpPass->begin(cmd, glm::vec4(0.01f, 0.01f, 0.03f, 1.0f));

        // Bind the pipeline for rendering
        mpRenderPipeline->bind(cmd);

        RenderPushConstant renderPushConstant = {
            .pointSize = mPointSize,
            .speedScale = mSpeedScale,
        };
        vkCmdPushConstants(cmd, mpRenderPipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof renderPushConstant, &renderPushConstant);

        // Bind the resources attached to the shader
        auto pShader = mpRenderPipeline->getShader();
        pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 0);
        pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 1);

        // Draw one point per particle, the vertex shader looks each one up by its vertex index
        vkCmdDraw(cmd, kParticleCount, 1, 0, 0);

        // Draw GUI
        App::renderGUI(cmd);

        // Submit command buffer to rasterizer and present swapchain frame
        mpPass->end(cmd);
        mpSwapchain->present(cmd, mpPass->getOutput());
    }

    void appGUI(ImGuiContext* pContext)
    {
        ImGui::SetCurrentContext(pContext);

        // Render the base GUI, the menu bar with its subwindows
        App::baseGUI(mpDevice, mpSwapchain, getPipelines());

        // Here we can add app-specific GUI elements
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Compute Particles")) {
            ImGui::Text("Particles: %d", kParticleCount);

            glm::uvec3 localSize = mpComputePipeline->getLocalSize();
            ImGui::Text("Local size: %d x %d x %d", localSize.x, localSize.y, localSize.z);

            ImGui::Separator();

            ImGui::Checkbox("Pause", &mPaused);
            ImGui::SliderFloat("Attraction", &mStrength, 0.0f, 50.0f, "%.1f");
            ImGui::SliderFloat("Damping", &mDamping, 0.0f, 2.0f, "%.2f");
            ImGui::SliderFloat("Point size", &mPointSize, 1.0f, 8.0f, "%.1f");
            ImGui::SliderFloat("Color speed", &mSpeedScale, 0.01f, 1.0f, "%.2f");

            if (ImGui::Button("Reset particles")) {
                setupParticles();
            }
        }

        ImGui::End();
    }

    void appKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods)
    {
        // Invoke the base application's keyboard commands
        App::baseKeyCallback(pWindow, key, scancode, action, mods, mpDevice, mpSwapchain, getPipelines());

        // Here we can add app-specific keyboard commands
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
            mPaused = !mPaused;
        }
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
    // Both pipelines are reloaded together, so that a shader edit picks up in the simulation and the rendering alike
    std::vector<std::shared_ptr<Pipeline>> getPipelines() const
    {
        return {mpRenderPipeline, mpComputePipeline};
    }

    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::shared_ptr<Pass> mpPass;
    std::shared_ptr<Pipeline> mpRenderPipeline;
    std::shared_ptr<ComputePipeline> mpComputePipeline;

    std::shared_ptr<Camera> mpCamera;

    std::shared_ptr<Buffer> mpParticleBuffer;

    glm::vec3 mAttractor = glm::vec3(0.0f);
    float mStrength = 20.0f;
    float mDamping = 0.15f;
    float mPointSize = 2.0f;
    float mSpeedScale = 0.2f;

    bool mPaused = false;
    float mTime = 0.0f;
    float mDelta = 0.0f;
};

int main()
{
    ComputeParticles app = ComputeParticles();
    app.run();
    return 0;
}
