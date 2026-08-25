#include "Mandrill.h"

using namespace Mandrill;

/// This is SampleApp without a window: the same rotating textured billboard, rendered by a device that has no
/// surface, no swapchain and no GUI, and written to a PNG file when the last frame is done.
///
/// Note that this class does not inherit App. App owns the window, the GLFW callbacks and the ImGUI context, and a
/// headless device has none of those, so the setup that App would have driven lives here instead, together with a
/// render loop of its own built on Helpers::cmdBegin() and Helpers::cmdEnd().
class HeadlessSample
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

    HeadlessSample(uint32_t width, uint32_t height)
    {
        Log::Info("=== Mandrill {} (headless) ===", MANDRILL_VERSION_STRING);

        // Create a Vulkan instance and device. Passing no window gives a headless device: no surface is created, the
        // swapchain extension is left out, and GLFW is never initialized. Nothing is presented, so frames are never
        // in flight with each other and one copy of every per-frame resource is enough.
        uint32_t framesInFlightCount = 1;
        mpDevice = make_ptr<Device>(nullptr, std::vector<const char*>(), nullptr,
                                    std::numeric_limits<uint32_t>::max(), framesInFlightCount);

        // Without a swapchain there is nothing that decides the resolution and the format, so the pass is given both.
        // Its attachments are the images that the app renders into and later reads back.
        VkExtent2D extent = {.width = width, .height = height};
        uint32_t colorAttachmentCount = 1;
        bool depthAttachment = true;
        mpPass = mpDevice->createPass(extent, VK_FORMAT_R8G8B8A8_UNORM, colorAttachmentCount, depthAttachment,
                                      mpDevice->getSampleCount());

        // Create a shader module with vertex and fragment shader
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("HeadlessSample/VertexShader.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("HeadlessSample/FragmentShader.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        auto pShader = mpDevice->createShader(shaderDesc);

        // Create a pipeline for rendering using the shader
        mpPipeline = mpDevice->createPipeline(mpPass, pShader, PipelineDesc());

        // Setup camera
        mpCamera = mpDevice->createCamera();
        mpCamera->setPosition(glm::vec3(0.0f, 0.0f, 5.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);
        mpCamera->setAspectRatio(static_cast<float>(extent.width) / static_cast<float>(extent.height));

        // Create a texture and bind a sampler to it
        mpTexture = mpDevice->createTextureFromFile(TextureType::Texture2D, VK_FORMAT_R8G8B8A8_UNORM,
                                                    GetResourcePath("textures/icon.png"));

        // Vertices in scene
        setupVertexBuffers();

        // Uniform for sending model matrix to shaders, with one copy per frame in flight
        mpUniform = mpDevice->createPerFrameBuffer(sizeof(glm::mat4));

        // Attach the resources to the names they have in the shader
        pShader->setResource("camera", mpCamera->getUniformBuffer());
        pShader->setResource("mesh", mpUniform);
        pShader->setResource("diffuseTexture", mpTexture);
    }

    // Render a fixed number of frames and save the last one.
    void run(uint32_t frameCount, float delta, const std::filesystem::path& output)
    {
        Log::Info("Rendering {} frames of {} x {}...", frameCount, mpPass->getExtent().width,
                  mpPass->getExtent().height);

        for (uint32_t i = 0; i < frameCount; i++) {
            update(delta);
            render();
        }

        // A pass leaves its output transitioned for transfer, which is what saving it expects
        mpPass->getOutput()->saveToPNG(output);
    }

private:
    void update(float delta)
    {
        mAngle += mRotationSpeed * delta;

        // There is no window to poll for input, so the camera only has to refresh its matrices
        mpCamera->update();

        glm::mat4 model = glm::rotate(glm::mat4(1.0f), mAngle, glm::vec3(0.0f, 1.0f, 0.0f));

        mpUniform->copyFromHost(&model);
    }

    void render()
    {
        // A one-time command buffer takes the place of the one the swapchain hands out. Helpers::cmdEnd() submits it
        // and waits for the queue to go idle, so the next frame is free to reuse every resource.
        VkCommandBuffer cmd = Helpers::cmdBegin(mpDevice);

        // Prepare rasterizer
        mpPass->begin(cmd, glm::vec4(0.0f, 0.4f, 0.2f, 1.0f));

        // Bind the pipeline for rendering
        mpPipeline->bind(cmd);

        // Turn off back-face culling
        vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

        // Bind the resources attached to the shader
        auto pShader = mpPipeline->getShader();
        pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 0);
        pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 1);

        // Bind vertex and index buffers
        VkBuffer vertexBuffer = mpVertexBuffer->getBuffer();
        VkDeviceSize vertexBufferOffset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &vertexBufferOffset);
        vkCmdBindIndexBuffer(cmd, mpIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // Draw mesh
        vkCmdDrawIndexed(cmd, count(mIndices), 1, 0, 0, 0);

        // There is nothing to present to, so the pass only transitions its output for the read-back
        mpPass->end(cmd);

        // Submit command buffer to rasterizer
        Helpers::cmdEnd(mpDevice, cmd);
    }

    std::shared_ptr<Device> mpDevice;
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

    std::shared_ptr<DynamicBuffer> mpUniform;
};

int main(int argc, char* argv[])
{
    // Resolution and frame count on the command line, since there is no window to resize and no key to press to stop
    uint32_t width = argc > 1 ? static_cast<uint32_t>(std::atoi(argv[1])) : 1280;
    uint32_t height = argc > 2 ? static_cast<uint32_t>(std::atoi(argv[2])) : 720;
    uint32_t frameCount = argc > 3 ? static_cast<uint32_t>(std::atoi(argv[3])) : 60;

    // The frames are not paced against the wall clock here, so the time step is picked rather than measured
    float delta = 1.0f / 60.0f;

    try {
        HeadlessSample app = HeadlessSample(width, height);
        app.run(frameCount, delta, "HeadlessSample.png");
    } catch (const std::exception& e) {
        Log::Error("{}", e.what());
        return 1;
    }

    return 0;
}
