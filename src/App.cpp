#include "App.h"

#include "Error.h"
#include "Helpers.h"
#include "Log.h"

#include "stb_image.h"

#ifdef MANDRILL_WINDOWS
// Include windows.h first
#include <windows.h>
// Line to stop formatter
#include <shellapi.h>
#endif

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

        // Mouse cursor movement
        mCursorDeltaX = mCursorX - mCursorPrevX;
        mCursorDeltaY = mCursorY - mCursorPrevY;
        mCursorPrevX = mCursorX;
        mCursorPrevY = mCursorY;

        auto& io = ImGui::GetIO();
        mKeyboardCapturedByGUI = io.WantCaptureKeyboard;
        mMouseCapturedByGUI = io.WantCaptureMouse;

        update(mDelta);

        ImGuiContext* pContext = newFrameGUI();

        appGUI(pContext);

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

void App::baseGUI(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                  std::shared_ptr<Pipeline> pPipeline, std::shared_ptr<Shader> pShader)
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

            if (ImGui::MenuItem("Reload shaders", "R")) {
                pShader->reload();
                pPipeline->recreate();
                pSwapchain->recreate();
            }

            if (ImGui::MenuItem("Take screenshot", "F12", false)) {
                Log::warning("Taking screenhots is currently not implemented");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {

            if (ImGui::MenuItem("Hide menu", "F2", false)) {
                mShowMainMenu = !mShowMainMenu;
            }

            if (ImGui::MenuItem("Frame rate", "F3", false)) {
                mShowFrameRate = !mShowFrameRate;
            }

            if (ImGui::MenuItem("Toggle fullscreen", "F11", false)) {
                Log::warning("Fullscreen is currently not implemented");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Show controls", "F1", false)) {
                mShowHelp = !mShowHelp;
            }

            if (ImGui::MenuItem("About", "", false)) {
                mShowAbout = !mShowAbout;
            }

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
            ImGui::Text("FPS: %.2f", 1.0f / mDeltaSmooth);
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }

    if (mShowHelp) {
        if (ImGui::Begin("Help", &mShowHelp)) {
            ImGui::Text("Camera movement:");
            ImGui::Text("\tW: Move forward");
            ImGui::Text("\tS: Move backward");
            ImGui::Text("\tA: Strafe left");
            ImGui::Text("\tD: Strafe right");
            ImGui::Text("\tArrow keys: Pan");
            ImGui::Text("\tPeriod: Zoom in");
            ImGui::Text("\tComma: Zoom out");
            ImGui::Text("\tShift: Speed up movement");
            ImGui::Text("\tCtrl: Slow down movement");
            ImGui::Text("\tLeft mouse button: Click and drag to pan");
            ImGui::Text("\tRight mouse button: Capture/release mouse for panning");
        }
        ImGui::End();
    }

    if (mShowAbout) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_AlwaysAutoResize;
        int width, height;
        glfwGetWindowSize(mpWindow, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(0.5f * width, 0.5f * height), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("About", &mShowAbout, flags)) {
            ImGui::Text("%s v%d.%d.%d", MANDRILL_NAME, MANDRILL_VERSION_MAJOR, MANDRILL_VERSION_MINOR,
                        MANDRILL_VERSION_PATCH);
            ImGui::Text(
                "This is an education and research graphics framework based on Vulkan, written and used at Lund "
                "University.");
            ImGui::Text(
                "Latest source code is available from the git repository and is released under the MIT License.");

#ifdef MANDRILL_WINDOWS
            if (ImGui::Button("Go to repo")) {
                ShellExecute(0, 0, "https://github.com/rikardolajos/Mandrill/tree/master", 0, 0, SW_SHOW);
            }
            ImGui::SameLine();
#endif

            if (ImGui::Button("Close")) {
                mShowAbout = false;
            }
        }
        ImGui::End();
    }
}

void App::renderGUI(VkCommandBuffer cmd)
{
    if (!mCreatedGUI) {
        return;
    }

    ImGui::PopFont();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void App::baseKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods,
                          std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                          std::shared_ptr<Pipeline> pPipeline, std::shared_ptr<Shader> pShader)
{
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        mShowHelp = !mShowHelp;
    }

    if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        mShowMainMenu = !mShowMainMenu;
    }

    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
        mShowFrameRate = !mShowFrameRate;
    }

    if (key == GLFW_KEY_V && action == GLFW_PRESS) {
        bool vsync = pDevice->getVsync();
        pDevice->setVsync(!vsync);
        pSwapchain->recreate();
    }

    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        pShader->reload();
        pPipeline->recreate();
        pSwapchain->recreate();
    }
}

void App::baseCursorPosCallback(GLFWwindow* pWindow, double xPos, double yPos)
{
    mCursorX = xPos;
    mCursorY = yPos;
}

void App::baseMouseButtonCallback(GLFWwindow* pWindow, int button, int action, int mods, std::shared_ptr<Camera> pCamera)
{
    if (button == GLFW_MOUSE_BUTTON_2 && action == GLFW_PRESS) {
        bool captured = pCamera->toggleMouseCapture();
        glfwSetInputMode(pWindow, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
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
    if (image.pixels) {
        glfwSetWindowIcon(mpWindow, 1, &image);
    } else {
        Log::error("Failed to load icon.png.");
    }

    if (!glfwVulkanSupported()) {
        Log::error("Failed to find Vulkan.");
        Check::GLFW();
    }

    glfwSetWindowUserPointer(mpWindow, this);
    glfwSetKeyCallback(mpWindow, keyCallbackEntry);
    glfwSetCursorPosCallback(mpWindow, cursorPosCallbackEntry);
    glfwSetMouseButtonCallback(mpWindow, mouseButtonCallbackEntry);
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

void App::keyCallbackEntry(GLFWwindow* pWindow, int key, int scancode, int action, int mods)
{
    App* pApp = static_cast<App*>(glfwGetWindowUserPointer(pWindow));
    pApp->appKeyCallback(pWindow, key, scancode, action, mods);
}

void App::cursorPosCallbackEntry(GLFWwindow* pWindow, double xPos, double yPos)
{
    App* pApp = static_cast<App*>(glfwGetWindowUserPointer(pWindow));
    pApp->appCursorPosCallback(pWindow, xPos, yPos);
}

void App::mouseButtonCallbackEntry(GLFWwindow* pWindow, int button, int action, int mods)
{
    App* pApp = static_cast<App*>(glfwGetWindowUserPointer(pWindow));
    pApp->appMouseButtonCallback(pWindow, button, action, mods);
}
