#include "App.h"

#include "Error.h"
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

    if (glfwInit() == GLFW_FALSE) {
        Log::error("Failed to initialze GLFW.");
        Check::GLFW();
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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

App::~App()
{
    glfwDestroyWindow(mpWindow);
    glfwTerminate();
}

void App::run()
{
    while (!glfwWindowShouldClose(mpWindow)) {
        render();

        drawUI();

        glfwPollEvents();
    }

    Log::info("Exiting...");
}

void App::drawUI(){

};

void App::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, 1);
    }
}