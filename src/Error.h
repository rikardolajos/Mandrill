#pragma once

#include "Common.h"

namespace Mandrill
{
    /// <summary>
    /// Namespace with error checking functions for Mandrill.
    /// </summary>
    namespace Check
    {
        /// <summary>
        /// Check for GLFW error
        /// </summary>
        /// <param name="loc">Source file location (leave at default)</param>
        void MANDRILL_API GLFW(const std::source_location loc = std::source_location::current());

        /// <summary>
        /// Check for Vulkan error
        /// </summary>
        /// <param name="res">Result returned from Vulkan function call</param>
        /// <param name="loc">Source file location (leave at default)</param>
        void MANDRILL_API Vk(VkResult res, const std::source_location loc = std::source_location::current());
    } // namespace Check
} // namespace Mandrill
