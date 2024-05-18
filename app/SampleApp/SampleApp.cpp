#include "Mandrill.h"

using namespace Mandrill;

class SampleApp : public App
{
public:
    void createSampler()
    {
        VkSamplerCreateInfo ci{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = mpDevice->getProperties().physicalDevice.limits.maxSamplerAnisotropy,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 1000.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };

        Check::Vk(vkCreateSampler(mpDevice->getDevice(), &ci, nullptr, &mSampler));
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
        layout.emplace_back(1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT); // Texture
        layout.emplace_back(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT); // Mesh model matrix
        mpPipeline = std::make_shared<Rasterizer>(mpDevice, mpSwapchain, layout, mpShader);

        createSampler();
    }

    ~SampleApp()
    {
        vkDestroySampler(mpDevice->getDevice(), mSampler, nullptr);
        mpPipeline.reset();
        mpDevice.reset();
    }

    void render() override
    {
        float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();
        mpPipeline->frameBegin(cmd, clearColor);

        // Render by building command buffer
        //vkCmdDraw(cmd, 3, 1, 0, 0);

        //vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 0, )

        mpPipeline->frameEnd(cmd);
        mpSwapchain->present();
    }

    void drawUI() override
    {
        // Draw common UI in base class
        App::drawUI();
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
            Log::info("You pressed space!");
        }
    }

private:
    std::shared_ptr<Device> mpDevice;
    std::shared_ptr<Swapchain> mpSwapchain;
    std::shared_ptr<Shader> mpShader;
    std::shared_ptr<Rasterizer> mpPipeline;

    VkSampler mSampler;
};

int main()
{
    SampleApp app = SampleApp();
    app.run();
    return 0;
}
