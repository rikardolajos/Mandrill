#pragma once

#include "Common.h"

#include "Device.h"
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
        virtual void renderGUI(ImGuiContext* pContext) = 0;

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
        /// screenshots.
        /// </summary>
        /// <param name="pDevice"></param>
        /// <param name="pSwapchain"></param>
        void renderGUI(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain);

        /// <summary>
        /// Draw the GUI  by writing the state to a command buffer.
        ///
        /// Call this while populating command buffer.
        /// </summary>
        /// <param name="cmd">Command buffer to write to</param>
        void drawGUI(VkCommandBuffer cmd);

        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

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

        // Time since last frame in seconds
        float mDelta = 0.0f;
        
        // Smoothed delta for GUI presentation
        float mDeltaSmooth = 0.0f;

        // Give new delta sample 5% weight
        const float kSmoothingFactor = 0.05f;

        // GUI
        bool mCreatedGUI = false;
        bool mShowMainMenu = true;
        bool mShowFrameRate = false;

        // Descriptor pool for ImGUI
        VkDescriptorPool mDescriptorPool;

        // Font for ImGUI
        ImFont* mFont;
    };
} // namespace Mandrill
