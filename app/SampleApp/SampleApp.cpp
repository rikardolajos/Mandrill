#include "Mandrill.h"

using namespace Mandrill;

class SampleApp : public App
{
public:
    void setupVertexBuffers()
    {
        mVertices.push_back({
            {-1.0f, -1.0f, 0.0f}, // pos
            {0.0f, 0.0f, 1.0f},   // normal
            {0.0f, 0.0f},         // uvCoord
        });

        mVertices.push_back({
            {1.0f, -1.0f, 0.0f}, // pos
            {0.0f, 0.0f, 1.0f},  // normal
            {1.0f, 0.0f},        // uvCoord
        });

        mVertices.push_back({
            {-1.0f, 1.0f, 0.0f}, // pos
            {0.0f, 0.0f, 1.0f},  // normal
            {0.0f, 1.0f},        // uvCoord
        });

        mVertices.push_back({
            {1.0f, 1.0f, 0.0f}, // pos
            {0.0f, 0.0f, 1.0f}, // normal
            {1.0f, 1.0f},       // uvCoord
        });

        mIndices = {0, 1, 3, 0, 3, 2};

        size_t verticesSize = mVertices.size() * sizeof(mVertices[0]);
        size_t indicesSize = mIndices.size() * sizeof(mIndices[0]);

        mpVertexBuffer = std::make_shared<Buffer>(mpDevice, verticesSize,
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        mpIndexBuffer = std::make_shared<Buffer>(mpDevice, indicesSize,
                                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        mpVertexBuffer->copyFromHost(mVertices.data(), verticesSize);
        mpIndexBuffer->copyFromHost(mIndices.data(), indicesSize);
    }

    VkWriteDescriptorSet getModelDescriptor(uint32_t binding)
    {
        std::array<glm::mat4, 1> matrices{mModel};

        mpUniform->copyFromHost(matrices.data(), matrices.size() * sizeof(glm::mat4));

        VkWriteDescriptorSet descriptor = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &mBufferInfo,
        };

        return descriptor;
    }

    SampleApp() : App("Sample App", 1280, 720)
    {
        // Create a the Vulkan instance and device
        mpDevice = std::make_shared<Device>(mpWindow);

        // Create a swapchain with 2 frames in flight
        mpSwapchain = std::make_shared<Swapchain>(mpDevice, 2);

        // Create a shader module with vertex and fragment shader
        std::vector<ShaderCreator> shaderCreator;
        shaderCreator.emplace_back("VertexShader.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderCreator.emplace_back("FragmentShader.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT);
        mpShader = std::make_shared<Shader>(mpDevice, shaderCreator);

        // Create rasterizer pipeline with layout matching the shader
        std::vector<LayoutCreator> layout;
        layout.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT); // Camera matrix
        layout.emplace_back(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // Texture
        layout.emplace_back(0, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT); // Mesh model matrix
        mpPipeline = std::make_shared<Rasterizer>(mpDevice, mpSwapchain, layout, mpShader);

        // Setup camera
        mpCamera = std::make_shared<Camera>(mpDevice, mpWindow);
        mpCamera->setPosition(glm::vec3(5.0f, 0.0f, 0.0f));
        mpCamera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        mpCamera->setFov(60.0f);

        // Create a texture and bind a sampler to it
        mpTexture = std::make_shared<Texture>(mpDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8A8_SRGB, "icon.png");
        mpSampler = std::make_shared<Sampler>(mpDevice);
        mpTexture->setSampler(mpSampler->getSampler());

        // Vertices in scene
        setupVertexBuffers();

        // Uniform for sending model matrix to shaders
        mpUniform =
            std::make_shared<Buffer>(mpDevice, sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        mBufferInfo.buffer = mpUniform->getBuffer();
        mBufferInfo.offset = 0;
        mBufferInfo.range = sizeof(glm::mat4);
    }

    ~SampleApp()
    {
    }

    void update(float delta)
    {
        mpCamera->update(delta);

        mAngle += 0.2f * delta;

        mModel = glm::rotate(glm::mat4(1.0f), mAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void render() override
    {
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();

        if (!cmd) {
            return;
        }

        mpPipeline->frameBegin(cmd, glm::vec4(0.0f, 0.4f, 0.2f, 1.0f));

        vkCmdSetCullMode(cmd, VK_CULL_MODE_NONE);

        std::array<VkWriteDescriptorSet, 3> descriptors;
        descriptors[0] = mpCamera->getDescriptor(0);
        descriptors[1] = mpTexture->getDescriptor(1);
        descriptors[2] = getModelDescriptor(2);
        vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 0,
                                  static_cast<uint32_t>(descriptors.size()), descriptors.data());

        std::array<VkBuffer, 1> vertexBuffers = {mpVertexBuffer->getBuffer()};
        std::array<VkDeviceSize, 1> offsets = {0};
        vkCmdBindVertexBuffers(cmd, 0, vertexBuffers.size(), vertexBuffers.data(), offsets.data());

        vkCmdBindIndexBuffer(cmd, mpIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd, mIndices.size(), 1, 0, 0, 0);

        mpPipeline->frameEnd(cmd);
        mpSwapchain->present();
    }

private:
    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::shared_ptr<Shader> mpShader;
    std::shared_ptr<Rasterizer> mpPipeline;

    std::shared_ptr<Camera> mpCamera;
    // std::shared_ptr<Scene> mpScene;

    std::shared_ptr<Sampler> mpSampler;
    std::shared_ptr<Texture> mpTexture;

    std::shared_ptr<Buffer> mpVertexBuffer;
    std::shared_ptr<Buffer> mpIndexBuffer;

    std::vector<Vertex> mVertices;
    std::vector<uint32_t> mIndices;

    float mAngle = 0;
    glm::mat4 mModel;
    std::shared_ptr<Buffer> mpUniform;
    VkDescriptorBufferInfo mBufferInfo;
};

int main()
{
    SampleApp app = SampleApp();
    app.run();
    return 0;
}
