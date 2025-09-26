#include "App.h"

#include "Error.h"
#include "Helpers.h"
#include "Log.h"

#include "stb_image.h"

#if MANDRILL_WINDOWS
// Include windows.h first
#include <windows.h>
// Line to stop formatter
#include <shellapi.h>
#endif

using namespace Mandrill;

static void errorCallback(int errorCode, const char* pDescription)
{
    Log::Error("GLFW error {}: {}", errorCode, pDescription);
}

App::App(const std::string& title, uint32_t width, uint32_t height) : mWidth(width), mHeight(height)
{
    Log::Info("=== Mandrill {}.{}.{} ===", MANDRILL_VERSION_MAJOR, MANDRILL_VERSION_MINOR, MANDRILL_VERSION_PATCH);

    Log::Info("Initializing GLFW");
    initGLFW(title, width, height);

    Log::Info("Initializing ImGUI");
    initImGUI();

#ifdef MANDRILL_USE_OPENVDB
    openvdb::initialize();
#endif

    mFullscreen = false;
}

App::~App()
{
#ifdef MANDRILL_USE_OPENVDB
    openvdb::uninitialize();
#endif

    glfwDestroyWindow(mpWindow);
    glfwTerminate();
}

void App::run()
{
    Log::Info("Running...");

    float prevTimeStamp = 0.0f;
    while (!glfwWindowShouldClose(mpWindow)) {
        mTime = static_cast<float>(glfwGetTime());
        mDelta = mTime - prevTimeStamp;
        prevTimeStamp = mTime;

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

    Log::Info("Exiting...");
}

void App::createGUI(ptr<Device> pDevice, ptr<Pass> pPass)
{
    // Create descriptor pool for ImGUI
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    };

    VkDescriptorPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = count(poolSizes),
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
        .MinImageCount = 2,
        .ImageCount = 2,
        .MSAASamples = pPass->getSampleCount(),
        .PipelineCache = VK_NULL_HANDLE,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = pPass->getPipelineRenderingCreateInfo(),
        .Allocator = nullptr,
    };

    ImGui_ImplVulkan_Init(&info);

    // Create fonts texture
    ImGuiIO& io = ImGui::GetIO();
    mFont = io.Fonts->AddFontFromFileTTF("Roboto.ttf", 16);

    ImGui_ImplVulkan_CreateFontsTexture();

    // Set Mandrill style from ImThemes
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6000000238418579f;
    style.WindowPadding = ImVec2(6.0f, 6.0f);
    style.WindowRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(5.0f, 1.0f);
    style.FrameRounding = 3.0f;
    style.FrameBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 20.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabMinSize = 20.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 1.0f;
    style.TabMinWidthForCloseButton = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] =
        ImVec4(0.8588235378265381f, 0.929411768913269f, 0.886274516582489f, 0.8799999952316284f);
    style.Colors[ImGuiCol_TextDisabled] =
        ImVec4(0.8588235378265381f, 0.929411768913269f, 0.886274516582489f, 0.2800000011920929f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1294117718935013f, 0.1372549086809158f, 0.168627455830574f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_PopupBg] =
        ImVec4(0.2000000029802322f, 0.2196078449487686f, 0.2666666805744171f, 0.8999999761581421f);
    style.Colors[ImGuiCol_Border] =
        ImVec4(0.5372549295425415f, 0.47843137383461f, 0.2549019753932953f, 0.1620000004768372f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2000000029802322f, 0.2196078449487686f, 0.2666666805744171f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 0.7799999713897705f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.2313725501298904f, 0.2000000029802322f, 0.2705882489681244f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.501960813999176f, 0.07450980693101883f, 0.2549019753932953f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] =
        ImVec4(0.2000000029802322f, 0.2196078449487686f, 0.2666666805744171f, 0.75f);
    style.Colors[ImGuiCol_MenuBarBg] =
        ImVec4(0.2000000029802322f, 0.2196078449487686f, 0.2666666805744171f, 0.4699999988079071f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.2000000029802322f, 0.2196078449487686f, 0.2666666805744171f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.08627451211214066f, 0.1490196138620377f, 0.1568627506494522f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 0.7799999713897705f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.8583691120147705f, 0.1768314242362976f, 0.4644536077976227f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.5278970003128052f, 0.1246108785271645f, 0.2938056886196136f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] =
        ImVec4(0.8588235378265381f, 0.1764705926179886f, 0.4627451002597809f, 1.0f);
    style.Colors[ImGuiCol_Button] =
        ImVec4(0.4666666686534882f, 0.7686274647712708f, 0.8274509906768799f, 0.1400000005960464f);
    style.Colors[ImGuiCol_ButtonHovered] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 0.8600000143051147f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 1.0f);
    style.Colors[ImGuiCol_Header] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 0.7599999904632568f);
    style.Colors[ImGuiCol_HeaderHovered] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 0.8600000143051147f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.501960813999176f, 0.07450980693101883f, 0.2549019753932953f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
    style.Colors[ImGuiCol_SeparatorHovered] =
        ImVec4(0.09803921729326248f, 0.4000000059604645f, 0.7490196228027344f, 0.7799999713897705f);
    style.Colors[ImGuiCol_SeparatorActive] =
        ImVec4(0.09803921729326248f, 0.4000000059604645f, 0.7490196228027344f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] =
        ImVec4(0.4666666686534882f, 0.7686274647712708f, 0.8274509906768799f, 0.03999999910593033f);
    style.Colors[ImGuiCol_ResizeGripHovered] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 0.7799999713897705f);
    style.Colors[ImGuiCol_ResizeGripActive] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.3476395010948181f, 0.1477094888687134f, 0.3021619021892548f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.7896995544433594f, 0.03728193417191505f, 0.3666666150093079f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.501960813999176f, 0.07450980693101883f, 0.2549019753932953f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] =
        ImVec4(0.06666667014360428f, 0.1019607856869698f, 0.1450980454683304f, 0.9724000096321106f);
    style.Colors[ImGuiCol_TabUnfocusedActive] =
        ImVec4(0.1333333402872086f, 0.2588235437870026f, 0.4235294163227081f, 1.0f);
    style.Colors[ImGuiCol_PlotLines] =
        ImVec4(0.8588235378265381f, 0.929411768913269f, 0.886274516582489f, 0.6299999952316284f);
    style.Colors[ImGuiCol_PlotLinesHovered] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] =
        ImVec4(0.8588235378265381f, 0.929411768913269f, 0.886274516582489f, 0.6299999952316284f);
    style.Colors[ImGuiCol_PlotHistogramHovered] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 1.0f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
    style.Colors[ImGuiCol_TableBorderStrong] =
        ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
    style.Colors[ImGuiCol_TableBorderLight] =
        ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
    style.Colors[ImGuiCol_TextSelectedBg] =
        ImVec4(0.4549019634723663f, 0.196078434586525f, 0.2980392277240753f, 0.4300000071525574f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.8999999761581421f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
    style.Colors[ImGuiCol_NavWindowingDimBg] =
        ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
    style.Colors[ImGuiCol_ModalWindowDimBg] =
        ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);

    mCreatedGUI = true;
}

void App::destroyGUI(ptr<Device> pDevice)
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

    ImGui::PushFont(mFont);

    return ImGui::GetCurrentContext();
}

void App::baseGUI(ptr<Device> pDevice, ptr<Swapchain> pSwapchain, ptr<Pipeline> pPipeline)
{
    std::vector<ptr<Pipeline>> pipelines;
    pipelines.push_back(pPipeline);
    App::baseGUI(pDevice, pSwapchain, pipelines);
}

void App::baseGUI(ptr<Device> pDevice, ptr<Swapchain> pSwapchain, std::vector<ptr<Pipeline>> pPipelines)
{
    if (!mCreatedGUI) {
        return;
    }

    if (mShowMainMenu && ImGui::BeginMainMenuBar()) {

        if (ImGui::BeginMenu("File")) {

            if (ImGui::MenuItem("Exit", "ESC / Alt + F4", false)) {
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
                for (auto& pipeline : pPipelines) {
                    if (pipeline) {
                        pipeline->recreate();
                    }
                }
                pSwapchain->recreate();
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
                toggleFullscreen();
            }

            if (ImGui::MenuItem("Take screenshot", "F12", false)) {
                takeScreenshot(pSwapchain);
            }

            if (ImGui::MenuItem("Reset to initial framesize", "", false)) {
                resetFramebufferSize();
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
                                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs;
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
            ImGui::Text("\tA: Move left");
            ImGui::Text("\tD: Move right");
            ImGui::Text("\tE: Move up");
            ImGui::Text("\tQ: Move down");
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

            if (ImGui::Button("Go to repo")) {
#if MANDRILL_WINDOWS
                ShellExecute(0, 0, "https://github.com/rikardolajos/Mandrill/tree/master", 0, 0, SW_SHOW);
#elif MANDRILL_LINUX
                if (std::system("xdg-open https://github.com/rikardolajos/Mandrill/tree/master")) {
                    Log::Error("Unable to open browser");
                }
#endif
            }

            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                mShowAbout = false;
            }
        }
        ImGui::End();
    }
}

void App::renderGUI(VkCommandBuffer cmd) const
{
    if (!mCreatedGUI) {
        return;
    }

    ImGui::PopFont();

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void App::baseKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods, ptr<Device> pDevice,
                          ptr<Swapchain> pSwapchain, ptr<Pipeline> pPipeline)
{
    std::vector<ptr<Pipeline>> pipelines;
    pipelines.push_back(pPipeline);
    App::baseKeyCallback(pWindow, key, scancode, action, mods, pDevice, pSwapchain, pipelines);
}

void App::baseKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods, ptr<Device> pDevice,
                          ptr<Swapchain> pSwapchain, std::vector<ptr<Pipeline>> pipelines)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(mpWindow, 1);
    }

    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        mShowHelp = !mShowHelp;
    }

    if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {
        mShowMainMenu = !mShowMainMenu;
    }

    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
        mShowFrameRate = !mShowFrameRate;
    }

    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        toggleFullscreen();
    }

    if (key == GLFW_KEY_F12 && action == GLFW_PRESS) {
        takeScreenshot(pSwapchain);
    }

    if (key == GLFW_KEY_V && action == GLFW_PRESS) {
        bool vsync = pDevice->getVsync();
        pDevice->setVsync(!vsync);
        pSwapchain->recreate();
    }

    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        for (auto& pipeline : pipelines) {
            if (pipeline) {
                pipeline->recreate();
            }
        }

        pSwapchain->recreate();
    }
}

void App::baseCursorPosCallback(GLFWwindow* pWindow, double xPos, double yPos)
{
    mCursorX = static_cast<float>(xPos);
    mCursorY = static_cast<float>(yPos);
}

void App::baseMouseButtonCallback(GLFWwindow* pWindow, int button, int action, int mods, ptr<Camera> pCamera)
{
    if (button == GLFW_MOUSE_BUTTON_2 && action == GLFW_PRESS && pCamera) {
        bool captured = pCamera->toggleMouseCapture();
        glfwSetInputMode(pWindow, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

        ImGuiIO& io = ImGui::GetIO();
        io.SetAppAcceptingEvents(!captured);
        if (captured) {
            ImGui::SetWindowFocus();
        }
    }
}

void App::initGLFW(const std::string& title, uint32_t width, uint32_t height)
{
    if (glfwInit() == GLFW_FALSE) {
        Log::Error("Failed to initialze GLFW");
        Check::GLFW();
    }

    glfwSetErrorCallback(errorCallback);

    // Save video mode of fullscreen mode
    mpMonitor = glfwGetPrimaryMonitor();
    if (!mpMonitor) {
        Log::Error("Failed to find primary monitor");
        Check::GLFW();
    }
    const GLFWvidmode* mode = glfwGetVideoMode(mpMonitor);
    std::memcpy(&mFullscreenMode, mode, sizeof(GLFWvidmode));

    glfwWindowHint(GLFW_RED_BITS, mFullscreenMode.redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mFullscreenMode.greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mFullscreenMode.blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mFullscreenMode.refreshRate);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    Check::GLFW();

    std::string fullTitle = std::format("Mandrill: {}", title);
    mpWindow = glfwCreateWindow(width, height, fullTitle.c_str(), nullptr, nullptr);
    if (!mpWindow) {
        Log::Error("Failed to create window");
        Check::GLFW();
    }

    GLFWimage image = {};
    image.pixels = stbi_load("icon.png", &image.width, &image.height, nullptr, 4);
    if (image.pixels) {
        glfwSetWindowIcon(mpWindow, 1, &image);
        stbi_image_free(image.pixels);
    } else {
        Log::Error("Failed to load icon.png");
    }

    if (!glfwVulkanSupported()) {
        Log::Error("Failed to find Vulkan");
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

static void requestScreenshot(ptr<Swapchain> pSwapchain)
{
    pSwapchain->requestScreenshot();
}

static void acquireAndSaveScreenshot(ptr<Swapchain> pSwapchain)
{
    // Acquire screenshot (wait until swapchain is ready)
    std::vector<uint8_t> data = pSwapchain->waitForScreenshot();

    // Save screenshot
    char timestamp[64];
    time_t t = std::time(nullptr);
    std::strftime(timestamp, 64, "%G-%m-%d_%H-%M-%S", std::localtime(&t));
    std::filesystem::path filename = std::format("Screenshot_{}.png", timestamp);
    stbi_write_png(filename.string().c_str(), pSwapchain->getExtent().width, pSwapchain->getExtent().height, 4,
                   data.data(), pSwapchain->getScreenshotImagePitch());
    auto fullpath = std::filesystem::current_path() / filename;

    Log::Info("Screenshot saved to {}", fullpath.string());
}

void App::takeScreenshot(ptr<Swapchain> pSwapchain)
{
    auto threadRequest = std::thread(requestScreenshot, pSwapchain);
    threadRequest.detach();

    auto threadAcquire = std::thread(acquireAndSaveScreenshot, pSwapchain);
    threadAcquire.detach();
}

void App::toggleFullscreen()
{
    if (mFullscreen) {
        // Set windowed
        int xpos = (mFullscreenMode.width - mWidth) / 2;
        int ypos = (mFullscreenMode.height - mHeight) / 2;
        glfwSetWindowMonitor(mpWindow, NULL, xpos, ypos, mWidth, mHeight, mFullscreenMode.refreshRate);
    } else {
        // Set fullscreen
        glfwSetWindowMonitor(mpWindow, mpMonitor, 0, 0, mFullscreenMode.width, mFullscreenMode.height,
                             mFullscreenMode.refreshRate);
    }
    Check::GLFW();
    mFullscreen = !mFullscreen;
}

void App::resetFramebufferSize()
{
    // Set windowed
    int xpos = (mFullscreenMode.width - mWidth) / 2;
    int ypos = (mFullscreenMode.height - mHeight) / 2;
    glfwSetWindowMonitor(mpWindow, NULL, xpos, ypos, mWidth, mHeight, mFullscreenMode.refreshRate);

    Check::GLFW();
    mFullscreen = false;
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
