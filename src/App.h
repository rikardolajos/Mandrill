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

        virtual void execute() = 0;
        virtual void renderUI();

    private:
        void* mpWindow;
    };
} // namespace Mandrill
