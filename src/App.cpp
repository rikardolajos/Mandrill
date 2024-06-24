#include "App.h"

#include "Error.h"
#include "Helpers.h"
#include "Log.h"

#include "stb_image.h"

using namespace Mandrill;

static void errorCallback(int errorCode, const char* pDescription)
{
    Log::error("GLFW error {}: {}", errorCode, pDescription);
}

App::App(const std::string& title, uint32_t width, uint32_t height)
{
    Log::info("=== Mandrill {}.{}.{} ===", MANDRILL_VERSION_MAJOR, MANDRILL_VERSION_MINOR, MANDRILL_VERSION_PATCH);

    Log::info("Initializing GLFW");
    initGLFW(title, width, height);

    Log::info("Initializing ImGUI");
    initImGUI();
}

App::~App()
{
    glfwDestroyWindow(mpWindow);
    glfwTerminate();
}

void App::run()
{
    Log::info("Running...");

    while (!glfwWindowShouldClose(mpWindow)) {
        mDelta = static_cast<float>(glfwGetTime());
        glfwSetTime(0.0);

        mDeltaSmooth = kSmoothingFactor * mDelta + (1 - kSmoothingFactor) * mDeltaSmooth;

        ImGuiContext* pContext = newFrameGUI();

        update(mDelta);

        renderGUI(pContext);

        render();

        glfwPollEvents();
    }

    Log::info("Exiting...");
}

void App::createGUI(std::shared_ptr<Device> pDevice, VkRenderPass renderPass)
{
    // Create descriptor pool for ImGUI
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    VkDescriptorPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    Check::Vk(vkCreateDescriptorPool(pDevice->getDevice(), &ci, nullptr, &mDescriptorPool));

    // Setup platform for ImGUI
    ImGui_ImplVulkan_InitInfo info = {
        .Instance = pDevice->getInstance(),
        .PhysicalDevice = pDevice->getPhysicalDevice(),
        .Device = pDevice->getDevice(),
        .QueueFamily = pDevice->getQueueFamily(),
        .Queue = pDevice->getQueue(),
        .DescriptorPool = mDescriptorPool,
        .RenderPass = renderPass,
        .MinImageCount = 2,
        .ImageCount = 2,
        .MSAASamples = pDevice->getSampleCount(),
        .PipelineCache = VK_NULL_HANDLE,
        .Subpass = 0,
        .Allocator = nullptr,
    };

    ImGui_ImplVulkan_Init(&info);

    // Create fonts texture
    ImGuiIO& io = ImGui::GetIO();
    mFont = io.Fonts->AddFontFromFileTTF("Roboto.ttf", 16);

    ImGui_ImplVulkan_CreateFontsTexture();

    // Set style
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(5, 5);
    style.WindowRounding = 5.0f;
    style.FramePadding = ImVec2(5, 5);
    style.FrameRounding = 4.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(12, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.IndentSpacing = 25.0f;
    style.ScrollbarSize = 15.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 5.0f;
    style.GrabRounding = 3.0f;

    mCreatedGUI = true;
}

void App::destroyGUI(std::shared_ptr<Device> pDevice)
{
    vkDeviceWaitIdle(pDevice->getDevice());

    ImGui_ImplVulkan_DestroyFontsTexture();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    vkDestroyDescriptorPool(pDevice->getDevice(), mDescriptorPool, nullptr);

    mCreatedGUI = false;
}

ImGuiContext* App::newFrameGUI()
{
    if (!mCreatedGUI) {
        return nullptr;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::StyleColorsLight();
    ImGui::PushFont(mFont);

    return ImGui::GetCurrentContext();
}

void App::renderGUI(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain)
{
    if (!mCreatedGUI) {
        return;
    }

    if (mShowMainMenu && ImGui::BeginMainMenuBar()) {

        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("Exit", "Alt + F4", false)) {
                glfwSetWindowShouldClose(mpWindow, GLFW_TRUE);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            bool vsync = pDevice->getVsync();
            if (ImGui::MenuItem("Verical sync", "V", &vsync)) {
                pDevice->setVsync(vsync);
                pSwapchain->recreate();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {

            if (ImGui::MenuItem("Hide menu", "F2", false)) {
                mShowMainMenu = !mShowMainMenu;
            }

            if (ImGui::MenuItem("Frame rate", "F6", false)) {
                mShowFrameRate = !mShowFrameRate;
            }

            ImGui::MenuItem("Take screenshot", "F12", false);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Show controls", "F1", false);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (mShowFrameRate) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoBackground;
        ImGui::SetNextWindowPos(ImVec2(10.0f, 30.0f), ImGuiCond_Appearing);
        if (ImGui::Begin("Frame rate", &mShowFrameRate, flags)) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            ImGui::Text("Frametime: %.2f ms", mDeltaSmooth * 1000.0f);
            ImGui::Text("FPS: %.2f ms", 1.0f / mDeltaSmooth);
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }
}

void App::drawGUI(VkCommandBuffer cmd)
{
    if (!mCreatedGUI) {
        return;
    }

    ImGui::PopFont();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void App::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, 1);
    }

    if (key == GLFW_KEY_F6 && action == GLFW_PRESS) {
        // mShowFrameRate = !mShowFrameRate;
    }
}

void App::initGLFW(const std::string& title, uint32_t width, uint32_t height)
{
    if (glfwInit() == GLFW_FALSE) {
        Log::error("Failed to initialze GLFW.");
        Check::GLFW();
    }

    glfwSetErrorCallback(errorCallback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    Check::GLFW();

    std::string fullTitle = std::format("Mandrill: {}", title);
    mpWindow = glfwCreateWindow(width, height, fullTitle.c_str(), nullptr, nullptr);
    if (!mpWindow) {
        Log::error("Failed to create window.");
        Check::GLFW();
    }

    GLFWimage image;
    image.pixels = stbi_load("icon.png", &image.width, &image.height, nullptr, 4);
    glfwSetWindowIcon(mpWindow, 1, &image);

    if (!glfwVulkanSupported()) {
        Log::error("Failed to find Vulkan.");
        Check::GLFW();
    }

    glfwSetKeyCallback(mpWindow, keyCallback);
    Check::GLFW();
}

void App::initImGUI()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    ImGui_ImplGlfw_InitForVulkan(mpWindow, true);
}
