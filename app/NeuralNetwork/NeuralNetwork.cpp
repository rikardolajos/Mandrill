#include "Mandrill.h"

#include <stdexcept>

using namespace Mandrill;

/// <summary>
/// Evaluates a small multi-layer perceptron per pixel with cooperative vector instructions
/// (VK_NV_cooperative_vector).
///
/// The network is a neural image field: it was trained to map a 2D coordinate to a colour, so running it over the
/// screen reproduces the image it was fitted to. Train one with app/NeuralNetwork/train.py, which writes the network
/// file this app loads.
///
/// Everything to do with the network itself is handled by Mandrill's MLP class, and the inference is a single call to
/// mlpForward() in MLP.glsl. What is specific to this app is only the encoding of the input coordinate and what the
/// three outputs are taken to mean.
/// </summary>
class NeuralNetwork : public App
{
public:
    // View modes, must match NeuralImage.frag
    enum Mode : uint32_t {
        MODE_NETWORK = 0,
        MODE_REFERENCE = 1,
        MODE_SPLIT = 2,
        MODE_DIFFERENCE = 3,
        MODE_COUNT = 4,
    };

    struct PushConstant {
        glm::vec2 resolution;
        glm::vec2 center;
        float zoom;
        float split;
        uint32_t mode;
        float differenceScale;
    };

    NeuralNetwork(const std::filesystem::path& networkPath) : App("Neural Network", 1024, 1024)
    {
        // Create a Vulkan instance and device. Cooperative vector needs its own extension, and the shader runs the
        // network in half precision, so the device has to be created with the features that go with that. Passing a
        // feature chain replaces the framework's default one, so everything the framework itself relies on is listed
        // here as well.
        std::vector<const char*> extensions = {
            VK_NV_COOPERATIVE_VECTOR_EXTENSION_NAME,
            // The ReLU in MLP.glsl builds a cooperative vector from a single value, which lands as a replicated
            // composite in the SPIR-V
            VK_EXT_SHADER_REPLICATED_COMPOSITES_EXTENSION_NAME,
        };

        VkPhysicalDeviceVulkan11Features vk11Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            // The weights are read as an array of float16_t through a buffer reference
            .storageBuffer16BitAccess = VK_TRUE,
        };

        VkPhysicalDeviceVulkan12Features vk12Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &vk11Features,
            .uniformAndStorageBuffer8BitAccess = VK_TRUE,
            .shaderFloat16 = VK_TRUE,
            .scalarBlockLayout = VK_TRUE,
            .timelineSemaphore = VK_TRUE,
            // The network is reached by device address rather than through a descriptor
            .bufferDeviceAddress = VK_TRUE,
            .vulkanMemoryModel = VK_TRUE,
            .vulkanMemoryModelDeviceScope = VK_TRUE,
        };

        VkPhysicalDeviceVulkan13Features vk13Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &vk12Features,
            .shaderDemoteToHelperInvocation = VK_TRUE,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE,
        };

        VkPhysicalDeviceVulkan14Features vk14Features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
            .pNext = &vk13Features,
            .dynamicRenderingLocalRead = VK_TRUE,
            .pushDescriptor = VK_TRUE,
        };

        VkPhysicalDeviceShaderReplicatedCompositesFeaturesEXT replicatedCompositesFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_REPLICATED_COMPOSITES_FEATURES_EXT,
            .pNext = &vk14Features,
            .shaderReplicatedComposites = VK_TRUE,
        };

        VkPhysicalDeviceCooperativeVectorFeaturesNV coopVecFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_VECTOR_FEATURES_NV,
            .pNext = &replicatedCompositesFeatures,
            .cooperativeVector = VK_TRUE,
        };

        VkPhysicalDeviceFeatures2 features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &coopVecFeatures,
            .features =
                {
                    .independentBlend = VK_TRUE,
                    .fillModeNonSolid = VK_TRUE,
                    .wideLines = VK_TRUE,
                    .largePoints = VK_TRUE,
                    .samplerAnisotropy = VK_TRUE,
                    .vertexPipelineStoresAndAtomics = VK_TRUE,
                    .fragmentStoresAndAtomics = VK_TRUE,
                    .shaderInt64 = VK_TRUE,
                },
        };

        mpDevice = make_ptr<Device>(mpWindow, extensions, &features2);

        // Create a swapchain
        mpSwapchain = mpDevice->createSwapchain();

        // Create a pass with 1 color attachment, only a fullscreen triangle is drawn into it
        mpPass = mpDevice->createPass(mpSwapchain->getExtent(), mpSwapchain->getImageFormat());

        // Load the network. Its shape decides the specialization constants of the shader that runs it, so it has to
        // be in place before the pipeline is created.
        mpMLP = make_ptr<MLP>(mpDevice, networkPath);
        if (!mpMLP->isValid()) {
            Log::Error("Unable to load a network from {}. Train one first:\n"
                       "    python app/NeuralNetwork/train.py",
                       networkPath.string());
            throw std::runtime_error("No network to run");
        }

        // Prepare vertex binding and attribute descriptions with empty vectors (only fullscreen triangles are used)
        std::vector<VkVertexInputBindingDescription> emptyBindingDescription;
        std::vector<VkVertexInputAttributeDescription> emptyAttributeDescription;
        PipelineDesc pipelineDesc = PipelineDesc(emptyBindingDescription, emptyAttributeDescription);
        pipelineDesc.depthTestEnable = VK_FALSE;

        // Create a pipeline, telling the shader the shape of the network that was loaded
        std::vector<ShaderDesc> shaderDesc;
        shaderDesc.emplace_back("NeuralNetwork/Fullscreen.vert", "main", VK_SHADER_STAGE_VERTEX_BIT);
        shaderDesc.emplace_back("NeuralNetwork/NeuralImage.frag", "main", VK_SHADER_STAGE_FRAGMENT_BIT,
                                mpMLP->getSpecializationInfo());
        auto pShader = mpDevice->createShader(shaderDesc);
        mpPipeline = mpDevice->createPipeline(mpPass, pShader, pipelineDesc);

        // The image the network was fitted to, kept around so that the two can be compared side by side
        mpReferenceImage = mpDevice->createTextureFromFile(TextureType::Texture2D, VK_FORMAT_R8G8B8A8_UNORM,
                                                           GetResourcePath("textures/icon.png"));

        // Attach the resources to the names they have in the shader
        mpMLP->attachTo(pShader);
        pShader->setResource("referenceImage", mpReferenceImage);

        // Initialize GUI
        App::createGUI(mpDevice, mpPass);
    }

    ~NeuralNetwork()
    {
        App::destroyGUI(mpDevice);
    }

    void update(float delta)
    {
        // Wait for GPU to finish rendering and using shared resources
        mpSwapchain->waitForFence();

        // Drag to pan. The cursor delta is in pixels, and the view maps the height of the window to one unit divided
        // by the zoom, so that is what it takes to make the image follow the cursor.
        if (mPanning && !mouseCapturedByGUI()) {
            glm::vec2 cursorDelta = getCursorDelta();
            mCenter -= cursorDelta / (static_cast<float>(mpSwapchain->getExtent().height) * mZoom);
        }
    }

    void render() override
    {
        // Check if attachments need to be updated
        if (mpSwapchain->recreated()) {
            mpPass->update(mpSwapchain->getExtent());
        }

        // Acquire frame from swapchain and prepare rasterizer
        VkCommandBuffer cmd = mpSwapchain->acquireNextImage();
        mpPass->begin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        // Bind the pipeline for rendering
        mpPipeline->bind(cmd);

        PushConstant pushConstant = {
            .resolution = glm::vec2(mpSwapchain->getExtent().width, mpSwapchain->getExtent().height),
            .center = mCenter,
            .zoom = mZoom,
            .split = mSplit,
            .mode = mMode,
            .differenceScale = mDifferenceScale,
        };
        vkCmdPushConstants(cmd, mpPipeline->getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof pushConstant,
                           &pushConstant);

        // Bind the resources attached to the shader, the network among them
        mpPipeline->getShader()->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);

        // Draw fullscreen triangle
        vkCmdDraw(cmd, 3, 1, 0, 0);

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
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Neural Network")) {
            ImGui::TextWrapped("%s", mpMLP->getPath().filename().string().c_str());
            ImGui::Text("Inputs: %d", static_cast<int>(mpMLP->getInputCount()));
            ImGui::Text("Outputs: %d", static_cast<int>(mpMLP->getOutputCount()));
            ImGui::Text("Layers: %d of width %d", static_cast<int>(mpMLP->getLayerCount()),
                        static_cast<int>(mpMLP->getHiddenWidth()));
            ImGui::Text("Frequencies: %d", static_cast<int>(mpMLP->getInputCount() - 2) / 4);

            ImGui::Separator();

            const char* modes[] = {"Network", "Reference", "Split", "Difference"};
            int mode = static_cast<int>(mMode);
            if (ImGui::Combo("Mode", &mode, modes, IM_ARRAYSIZE(modes))) {
                mMode = static_cast<uint32_t>(mode);
            }

            if (mMode == MODE_SPLIT) {
                ImGui::SliderFloat("Split", &mSplit, 0.0f, 1.0f, "%.2f");
            }

            if (mMode == MODE_DIFFERENCE) {
                ImGui::SliderFloat("Amplification", &mDifferenceScale, 1.0f, 20.0f, "%.1f");
            }

            ImGui::Separator();

            // Zooming out shows what the network does outside the unit square it was trained on
            ImGui::SliderFloat("Zoom", &mZoom, 0.1f, 20.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat2("Center", &mCenter.x, -2.0f, 2.0f, "%.2f");
            ImGui::TextDisabled("Drag with the left mouse button to pan");

            if (ImGui::Button("Reset view")) {
                mZoom = 1.0f;
                mCenter = glm::vec2(0.0f);
            }
        }

        ImGui::End();
    }

    void appKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods)
    {
        // Invoke the base application's keyboard commands
        App::baseKeyCallback(pWindow, key, scancode, action, mods, mpDevice, mpSwapchain, mpPipeline);

        // Here we can add app-specific keyboard commands
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
            mMode = (mMode + 1) % MODE_COUNT;
        }
    }

    void appCursorPosCallback(GLFWwindow* pWindow, double xPos, double yPos)
    {
        App::baseCursorPosCallback(pWindow, xPos, yPos);
    }

    void appMouseButtonCallback(GLFWwindow* pWindow, int button, int action, int mods)
    {
        // There is no camera to capture the cursor for, the view is panned instead
        App::baseMouseButtonCallback(pWindow, button, action, mods, nullptr);

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            mPanning = action == GLFW_PRESS && !mouseCapturedByGUI();
        }
    }

private:
    ptr<Device> mpDevice;
    ptr<Swapchain> mpSwapchain;
    ptr<Pass> mpPass;
    ptr<Pipeline> mpPipeline;

    ptr<MLP> mpMLP;
    ptr<Texture> mpReferenceImage;

    glm::vec2 mCenter = glm::vec2(0.0f);
    float mZoom = 1.0f;
    float mSplit = 0.5f;
    float mDifferenceScale = 5.0f;
    uint32_t mMode = MODE_SPLIT;
    bool mPanning = false;
};

int main(int argc, char* argv[])
{
    // The network the training script writes by default, or one given on the command line
    std::filesystem::path networkPath =
        argc > 1 ? std::filesystem::path(argv[1]) : GetResourcePath("networks/icon.mlp");

    try {
        NeuralNetwork app = NeuralNetwork(networkPath);
        app.run();
    } catch (const std::exception& e) {
        Log::Error("{}", e.what());
        return 1;
    }

    return 0;
}
