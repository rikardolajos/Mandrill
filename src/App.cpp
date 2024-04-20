#include "App.h"

#include "Log.h"

#include "stb_image.h"

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
        logInfo("Initializing GLFW");

        if (glfwInit() == GLFW_FALSE) {
            logError("Failed to initialze GLFW.");
        }

        std::string fullTitle = std::format("Mandrill: {}", title);
        mpWindow = glfwCreateWindow(width, height, fullTitle.c_str(), nullptr, nullptr);
        if (!mpWindow) {
            logError("Failed to create window.");
        }

        GLFWimage image;
        int c;
        image.pixels = stbi_load("icon.png", &image.width, &image.height, &c, 4);
        glfwSetWindowIcon(static_cast<GLFWwindow*>(mpWindow), 1, &image);
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

            App::renderUI();
            renderUI();

            glfwPollEvents();
        }
    }

    void App::renderUI()
    {
        
    };

} // namespace Mandrill
