#pragma once

#include "Common.h"

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
        //virtual void onMouseEvent() = 0;

        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

        GLFWwindow* mpWindow;
    };
} // namespace Mandrill
