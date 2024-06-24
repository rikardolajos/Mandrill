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
        /// Create a new Mandrill app
        /// </summary>
        /// <param name="title">Title to be shown in window frame</param>
        /// <param name="width">Width of window</param>
        /// <param name="height">Height of window</param>
        App(const std::string& title, uint32_t width = 1280, uint32_t height = 720);
        ~App();

        void run();

    protected:
        virtual void update(float delta) = 0;
        virtual void render() = 0;
        virtual void renderGUI() = 0;
        // virtual void onMouseEvent() = 0;

        void createGUI(std::shared_ptr<Device> pDevice, VkRenderPass renderPass);
        void destroyGUI(std::shared_ptr<Device> pDevice);
        void renderGUI(std::shared_ptr<Device> pDevice, std::shared_ptr<Swapchain> pSwapchain);
        void presentGUI(VkCommandBuffer cmd);

        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

        GLFWwindow* mpWindow;

    private:
        void initGLFW(const std::string& title, uint32_t width, uint32_t height);
        void initImGUI();

        void newFrameGUI();

        float mDelta = 0.0f;

        // GUI
        bool mCreatedGUI = false;
        bool mShowMainMenu = true;
        bool mShowFrametime = false;

        // Descriptor pool for ImGUI
        VkDescriptorPool mDescriptorPool;

        // Font for ImGUI
        ImFont* mFont;
    };
} // namespace Mandrill
