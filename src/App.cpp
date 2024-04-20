#include "App.h"

#include "Log.h"

//#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


namespace Mandrill
{
    static void errorCallback(int errorCode, const char* pDescription)
    {
        logError("GLFW error {}: {}", errorCode, pDescription);
    }

    App::App(const std::string& title, uint32_t width, uint32_t height)
    {
        logInfo("Initializing GLFW\n");

        if (glfwInit() == GLFW_FALSE) {
            logError("Failed to initialze GLFW.\n");
        }

        std::string fullTitle = std::format("Mandrill: {}", title);
        mpWindow = glfwCreateWindow(width, height, fullTitle.c_str(), nullptr, nullptr);
        if (!mpWindow) {
            logError("Failed to create window.\n");
        }
    }


    App::~App()
    {
        glfwDestroyWindow(static_cast<GLFWwindow*>(mpWindow));
    }

    void App::run()
    {
        while (!glfwWindowShouldClose(static_cast<GLFWwindow*>(mpWindow))) {
            // Handle inputs

            execute();

            renderUI();

            glfwPollEvents();
        }
    }

    void App::renderUI()
    {
        logInfo("App render");
    };

} // namespace Mandrill
