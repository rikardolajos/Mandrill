#include "Mandrill.h"

using namespace Mandrill;

class SampleApp : public App
{
public:
    void setupVertexBuffers()
    {
        mVertices.push_back({
            {-1.0f, -1.0f, 0.0f}, // position
            {0.0f, 0.0f, 1.0f},   // normal
            {0.0f, 0.0f},         // texcoord
            {1.0f, 0.0f, 0.0f},   // tangent
            {0.0f, 1.0f, 0.0f},   // binormal
        });

        mVertices.push_back({
            {1.0f, -1.0f, 0.0f}, // position
            {0.0f, 0.0f, 1.0f},  // normal
            {1.0f, 0.0f},        // texcoord
            {1.0f, 0.0f, 0.0f},  // tangent
            {0.0f, 1.0f, 0.0f},  // binormal
        });

        mVertices.push_back({
            {-1.0f, 1.0f, 0.0f}, // position
            {0.0f, 0.0f, 1.0f},  // normal
            {0.0f, 1.0f},        // texcoord
            {1.0f, 0.0f, 0.0f},  // tangent
            {0.0f, 1.0f, 0.0f},  // binormal
        });

        mVertices.push_back({
            {1.0f, 1.0f, 0.0f}, // position
            {0.0f, 0.0f, 1.0f}, // normal
            {1.0f, 1.0f},       // texcoord
            {1.0f, 0.0f, 0.0f}, // tangent
            {0.0f, 1.0f, 0.0f}, // binormal
        });

        mIndices = {0, 1, 3, 0, 3, 2};

        size_t verticesSize = mVertices.size() * sizeof(mVertices[0]);
        size_t indicesSize = mIndices.size() * sizeof(mIndices[0]);

        mpVertexBuffer =
            mpDevice->createBuffer(verticesSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        mpIndexBuffer =
            mpDevice->createBuffer(indicesSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mpVertexBuffer->copyFromHost(mVertices.data(), verticesSize);
        mpIndexBuffer->copyFromHost(mIndices.data(), indicesSize);
    }

    SampleApp() : App("Sample App", 1280, 720)
    {
        // Create a Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight (default)
        mpSwapchain = mpDevice->createSwapchain();

        // Create a pass with 1 color attachment, depth attachment and multisampling
        uint32_t colorAttachmentCount = 1;
        bool depthAttachment = true;
        mpPass = mpDevice->createPass(mpSwapchain->getExtent(), mpSwapchain->getImageFormat(), colorAttachmentCount,
                                      depthAttachment, mpDevice->getSampleCount());

        // Create a shader module with vertex and fragment shader
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("SampleApp/VertexShader.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("SampleApp/FragmentShader.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pShader = mpDevice->createShader(shaderDesc);

        // Create a pipeline for rendering using the shader
        mpPipeline = mpDevice->createPipeline(mpPass, pShader, PipelineDesc());

        // Setup camera
        mpCamera = mpDevice->createCamera(mpWindow, mpSwapchain);
        mpCamera->createDescriptor(VK_SHADER_STAGE_VERTEX_BIT);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Create a texture and bind a sampler to it
        mpTexture = mpDevice->createTexture(TextureType::Texture2D, VK_FORMAT_R8G8B8A8_UNORM,
                                            GetResourcePath("textures/icon.png"));

        // Vertices in scene
        setupVertexBuffers();

        // Uniform for sending model matrix to shaders
        VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
        VkDeviceSize size = Helpers::alignTo(sizeof(glm::mat4), alignment) * mpSwapchain->getFramesInFlightCount();
        mpUniform = mpDevice->createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Descriptor set for model matrix and texture
        std::vector<DescriptorDesc> descriptorDesc;
        descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, mpUniform, 0, sizeof(glm::mat4));
        descriptorDesc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mpTexture);
        mpDescriptor = mpDevice->createDescriptor(descriptorDesc, pShader->getDescriptorSetLayout(1));

        // Initialize GUI
        App::createGUI(mpDevice, mpPass);
    }

    ~SampleApp()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta)
    {
        // Wait for GPU to finish rendering and using shared resources
        mpSwapchain->waitForFence();

        if (!keyboardCapturedByGUI() && !mouseCapturedByGUI()) {
            mpCamera->update(delta, getCursorDelta());
        }

        mAngle += mRotationSpeed * delta;

        glm::mat4 model = glm::rotate(glm::mat4(1.0f), mAngle, glm::vec3(0.0f, 1.0f, 0.0f));

        VkDeviceSize offset = sizeof(glm::mat4) * mpSwapchain->getInFlightIndex();
        mpUniform->copyFromHost(&model, sizeof(glm::mat4), offset);
    }

    void render() override
    {
        // Check if camera matrix and attachments need to be updated
        if (mpSwapchain->recreated()) {
            mpCamera->updateAspectRatio();
            mpPass->update(mpSwapchain->getExtent());
        }

        // Acquire frame from swapchain and prepare rasterizer
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();
        mpPass->begin(cmd, glm::vec4(0.0f, 0.4f, 0.2f, 1.0f));

        // Bind the pipeline for rendering
        mpPipeline->bind(cmd);

        // Turn off back-face culling
        vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

        // Bind descriptor sets
        VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
        uint32_t cameraDescriptorOffset = static_cast<uint32_t>(Helpers::alignTo(sizeof(CameraMatrices), alignment) *
                                                                mpSwapchain->getInFlightIndex());
        mpCamera->getDescriptor()->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 0,
                                        cameraDescriptorOffset);

        uint32_t sceneDescriptorOffset =
            static_cast<uint32_t>(Helpers::alignTo(sizeof(glm::mat4), alignment) * mpSwapchain->getInFlightIndex());
        mpDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 1, sceneDescriptorOffset);

        // Bind vertex and index buffers
        VkBuffer vertexBuffer = mpVertexBuffer->getBuffer();
        VkDeviceSize vertexBufferOffset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &vertexBufferOffset);
        vkCmdBindIndexBuffer(cmd, mpIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Draw mesh
        vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mIndices.size()), 1, 0, 0, 0);

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
        App::baseGUI(mpDevice, mpSwapchain, mpPipeline);

        // Here we can add app-specific GUI elements
        if (ImGui::Begin("Sample App GUI")) {
            ImGui::Text("Rotation speed:");
            ImGui::SliderFloat("rad/s", &mRotationSpeed, 0.0f, 2.0f, "%.2f");
        }

        ImGui::End();
    }

    void appKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        // Invoke the base application's keyboard commands
        App::baseKeyCallback(window, key, scancode, action, mods, mpDevice, mpSwapchain, mpPipeline);

        // Here we can add app-specific keyboard commands
        if (key == GLFW_KEY_O && action == GLFW_PRESS) {
            mRotationSpeed -= 0.2f;
        }

        if (key == GLFW_KEY_P && action == GLFW_PRESS) {
            mRotationSpeed += 0.2f;
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
    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::shared_ptr<Pass> mpPass;
    std::shared_ptr<Pipeline> mpPipeline;

    std::shared_ptr<Camera> mpCamera;

    std::shared_ptr<Texture> mpTexture;

    std::shared_ptr<Buffer> mpVertexBuffer;
    std::shared_ptr<Buffer> mpIndexBuffer;

    std::vector<Vertex> mVertices;
    std::vector<uint32_t> mIndices;

    float mRotationSpeed = 0.2f;
    float mAngle = 0.0f;

    std::shared_ptr<Buffer> mpUniform;
    std::shared_ptr<Descriptor> mpDescriptor;
};

int main()
{
    SampleApp app = SampleApp();
    app.run();
    return 0;
}
