#pragma once

#include "Common.h"

#include "Camera.h"
#include "Device.h"
#include "Pipelines/Pipeline.h"
#include "Shader.h"
#include "Swapchain.h"

namespace Mandrill
{

    class MANDRILL_API App
    {
    public:
        /// <summary>
        /// Create a new Mandrill app.
        ///
        /// This class should be inherited by any application using Mandrill, and all the pure virtual functions have to
        /// be overriden.
        ///
        /// </summary>
        /// <param name="title">Title to be shown in the window frame</param>
        /// <param name="width">Width of window</param>
        /// <param name="height">Height of window</param>
        App(const std::string& title, uint32_t width = 1280, uint32_t height = 720);
        ~App();

        /// <summary>
        /// This is the main function of an application. A typical usecase would be to inherit <c>App</c> and then
        /// invoke the <c>run()</c> function. For a class <c>SampleApp</c> that inherits <c>App</c> this is how it could
        /// be used:
        ///
        /// <code>
        /// int main()
        /// {
        ///     SampleApp app = SampleApp();
        ///     app.run();
        ///     return 0;
        /// }
        /// </code>
        /// </summary>
        void run();

    protected:
        /// <summary>
        /// Update scene objects.
        ///
        /// This function is called once per <c>run()</c> loop and can be used to update scene objects like meshes and
        /// cameras that should animate.
        ///
        /// This function should be overridden.
        /// </summary>
        /// <param name="delta">Time passed since last frame</param>
        virtual void update(float delta) = 0;

        /// <summary>
        /// Render scene.
        ///
        /// This function is called once per <c>run()</c> loop is used to populate the command buffer for rendering.
        /// Typical use case to acquire a frame from the swapchain, populate the command buffer and then present the to
        /// the screen using a pipeline.
        ///
        /// This is an example:
        ///
        /// <code>
        /// void render() override
        /// {
        ///     VkCommandBuffer cmd = mpSwapchain->acquireNextImage();
        ///     mpPipeline->frameBegin(cmd, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        ///
        ///     // Populate command buffer using cmd here...
        ///
        ///     mpPipeline->frameEnd(cmd);
        ///     mpSwapchain->present();
        /// }
        /// </code>
        ///
        /// This function should be overridden.
        /// </summary>
        virtual void render() = 0;

        /// <summary>
        /// Draw application specific GUI of a Mandrill application.
        ///
        /// Use this function to draw GUI that is specific to the application. This function gives access to the current
        /// ImGUI context being used by Mandrill. Make sure to set the context before interacting with ImGUI:
        ///
        /// <code>
        /// ImGui::SetCurrentContext(pContext);
        /// </code>
        ///
        /// This function should be overridden.
        /// </summary>
        /// <param name="pContext">Current ImGUI context</param>
        virtual void appGUI(ImGuiContext* pContext) = 0;

        // virtual void onMouseEvent() = 0;

        /// <summary>
        /// Create a GUI for the application.
        ///
        /// This initializes all the resources for ImGUI.
        /// </summary>
        /// <param name="pDevice">Device to use for resource allocations</param>
        /// <param name="renderPass">Render pass that will be used for GUI presentation</param>
        void createGUI(std::shared_ptr<Device> pDevice, VkRenderPass renderPass);

        /// <summary>
        /// Destroy the GUI for the application.
        ///
        /// This will clean-up all the allocated resources of ImGUI.
        /// </summary>
        /// <param name="pDevice">Device that was used for resources allocations</param>
        void destroyGUI(std::shared_ptr<Device> pDevice);

        /// <summary>
        /// Draw the base GUI of a Mandrill application.
        ///
        /// This function will draw a menu bar and allow for basic features like showing framerate and taking
        /// screenshots. The menu also allows for controlling different aspects of the rendering context and
        /// therefore needs access to the device, swapchain, pipeline and shaders.
        /// </summary>
        /// <param name="pDevice">Device to toggle vertical sync</param>
        /// <param name="pSwapchain">Swapchain that should be recreated on changes</param>
        /// <param name="pPipeline">Pipeline that should be recreated on changes</param>
        /// <param name="pShader">Shader that should be reloaded</param>
        void baseGUI(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                     std::shared_ptr<Pipeline> pPipeline, std::shared_ptr<Shader> pShader);

        /// <summary>
        /// Render the GUI by writing the state to a command buffer.
        ///
        /// Call this while populating command buffer.
        /// </summary>
        /// <param name="cmd">Command buffer to write to</param>
        void renderGUI(VkCommandBuffer cmd);

        /// <summary>
        /// App keyboard callback function. This will handle keyboard commands associated with the base application
        /// menus. Read more in the GLFW documentation: https://www.glfw.org/docs/3.3/input_guide.html#input_key
        /// </summary>
        /// <param name="pWindow">The window that received the event</param>
        /// <param name="key">The keyboard key that was pressed or released</param>
        /// <param name="scancode">The system-specific scancode of the key</param>
        /// <param name="action">`GLFW_PRESS`, `GLFW_RELEASE` or `GLFW_REPEAT`. Future releases may add more
        /// actions</param> <param name="mods">Bit field describing which modifier keys were held down</param>
        /// <param name="pDevice">Device to toggle vertical sync</param>
        /// <param name="pSwapchain">Swapchain that should be recreated on changes</param>
        /// <param name="pPipeline">Pipeline that should be recreated on changes</param>
        /// <param name="pShader">Shader that should be reloaded</param>
        void baseKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods,
                             std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain,
                             std::shared_ptr<Pipeline> pPipeline, std::shared_ptr<Shader> pShader);

        /// <summary>
        /// Virtual function for app to override. Just invoke <code>baseKeyCallback()</code> to get standard
        /// keybindings. See <code>baseKeyCallback()</code> for more details.
        /// </summary>
        /// <param name="pWindow">The window that received the event</param>
        /// <param name="key">The keyboard key that was pressed or released</param>
        /// <param name="scancode">The system-specific scancode of the key</param>
        /// <param name="action">`GLFW_PRESS`, `GLFW_RELEASE` or `GLFW_REPEAT`. Future releases may add more
        /// actions</param> <param name="mods">Bit field describing which modifier keys were held down</param>
        virtual void appKeyCallback(GLFWwindow* pWindow, int key, int scancode, int action, int mods) = 0;

        void baseCursorPosCallback(GLFWwindow* pWindow, double xPos, double yPos);

        virtual void appCursorPosCallback(GLFWwindow* pWindow, double xPos, double yPos) = 0;

        void baseMouseButtonCallback(GLFWwindow* pWindow, int button, int action, int mods, std::shared_ptr<Camera> pCamera);

        virtual void appMouseButtonCallback(GLFWwindow* pWindow, int button, int action, int mods) = 0;

        glm::vec2 getCursorDelta()
        {
            return {mCursorDeltaX, mCursorDeltaY};
        }

        // GLFW window
        GLFWwindow* mpWindow;

    private:
        /// <summary>
        /// Initialize GLFW.
        ///
        /// Create a GLFW context with window for the application.
        /// </summary>
        /// <param name="title">Title to be shown in window frame</param>
        /// <param name="width">Width of the window</param>
        /// <param name="height">Height of the window</param>
        void initGLFW(const std::string& title, uint32_t width, uint32_t height);

        /// <summary>
        /// Initialize ImGUI.
        ///
        /// Create a ImGUI context for the application and initiate the Vulkan backend for ImGUI.
        /// </summary>
        void initImGUI();

        /// <summary>
        /// Request a new frame from the GUI.
        /// </summary>
        /// <returns></returns>
        ImGuiContext* newFrameGUI();

        /// <summary>
        /// GLFW keyboard callback entry function. This will invoke the app's overriden callback function.
        /// </summary>
        /// <param name="window"></param>
        /// <param name="key"></param>
        /// <param name="scancode"></param>
        /// <param name="action"></param>
        /// <param name="mods"></param>
        static void keyCallbackEntry(GLFWwindow* pWindow, int key, int scancode, int action, int mods);

        /// <summary>
        /// GLFW cursor position callback entry function. This will invoke the app's overridden callback function.
        /// </summary>
        /// <param name="pWindow"></param>
        /// <param name="xPos"></param>
        /// <param name="yPos"></param>
        static void cursorPosCallbackEntry(GLFWwindow* pWindow, double xPos, double yPos);

        /// <summary>
        /// GLFW mouse button callback entry function. This will invoke the app's overridden callback funciton.
        /// </summary>
        /// <param name="pWindow"></param>
        /// <param name="button"></param>
        /// <param name="action"></param>
        /// <param name="mods"></param>
        static void mouseButtonCallbackEntry(GLFWwindow* pWindow, int button, int action, int mods);

        // Time since last frame in seconds
        float mDelta = 0.0f;

        // Smoothed delta for GUI presentation
        float mDeltaSmooth = 0.0f;

        // Give new delta sample 5% weight
        const float kSmoothingFactor = 0.05f;

        // Mouse movement
        float mCursorX, mCursorY;
        float mCursorDeltaX, mCursorDeltaY;
        float mCursorPrevX, mCursorPrevY;

        // GUI
        bool mCreatedGUI = false;
        bool mShowMainMenu = true;
        bool mShowFrameRate = false;
        bool mShowHelp = false;
        bool mShowAbout = false;

        // Descriptor pool for ImGUI
        VkDescriptorPool mDescriptorPool;

        // Font for ImGUI
        ImFont* mFont;
    };
} // namespace Mandrill
