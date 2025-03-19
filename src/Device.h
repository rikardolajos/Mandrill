#pragma once

#include "Common.h"

namespace Mandrill
{
    struct MANDRILL_API DeviceProperties {
        VkPhysicalDeviceProperties physicalDevice;
        VkPhysicalDeviceMemoryProperties memory;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipeline;
    };

    class Device
    {
    public:
        MANDRILL_NON_COPYABLE(Device)

        /// <summary>
        /// Device that initializes and keeps track of everything related to rendering.
        /// </summary>
        /// <param name="window">GLFW window to create render context for</param>
        /// <param name="extensions">Extra device extensions to activate</param>
        /// <param name="physicalDeviceIndex">Physical device to use. Set this explicitly if device 0 is not the
        /// intended one.</param>
        MANDRILL_API Device(GLFWwindow* pWindow,
                            const std::vector<const char*>& extensions = std::vector<const char*>(),
                            uint32_t physicalDeviceIndex = 0);
        
        /// <summary>
        /// Destructor for device.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API ~Device();

        /// <summary>
        /// Get Vulkan device handle.
        /// </summary>
        /// <returns>Vulkan device handle</returns>
        MANDRILL_API VkDevice getDevice() const
        {
            return mDevice;
        }

        /// <summary>
        /// Get Vulkan instance handle.
        /// </summary>
        /// <returns>Vulkan instance handle</returns>
        MANDRILL_API VkInstance getInstance() const
        {
            return mInstance;
        }

        /// <summary>
        /// Get Vulkan physical device handle.
        /// </summary>
        /// <returns>Vulkan physical device handle</returns>
        MANDRILL_API VkPhysicalDevice getPhysicalDevice() const
        {
            return mPhysicalDevice;
        }

        /// <summary>
        /// Get device and physical device properties.
        /// </summary>
        /// <returns>DeviceProperties struct containing properties</returns>
        MANDRILL_API DeviceProperties getProperties() const
        {
            return mProperties;
        }

        /// <summary>
        /// Get the window that the device is bound to.
        /// </summary>
        /// <returns>A GLFWwindow pointer</returns>
        MANDRILL_API GLFWwindow* getWindow() const
        {
            return mpWindow;
        }

        /// <summary>
        /// Get the presentation surface of the application.
        /// </summary>
        /// <returns>The current VkSurfaceKHR</returns>
        MANDRILL_API VkSurfaceKHR getSurface() const
        {
            return mSurface;
        }

        /// <summary>
        /// Get the command pool.
        /// </summary>
        /// <returns>A Vulkan command pool handle</returns>
        MANDRILL_API VkCommandPool getCommandPool() const
        {
            return mCommandPool;
        }

        /// <summary>
        /// Get the queue.
        /// </summary>
        /// <returns>A Vulkan queue handle</returns>
        MANDRILL_API VkQueue getQueue() const
        {
            return mQueue;
        }

        /// <summary>
        /// Get the queue family.
        /// </summary>
        /// <returns>A Vulkan queue family handle</returns>
        MANDRILL_API uint32_t getQueueFamily() const
        {
            return mQueueFamilyIndex;
        }

        /// <summary>
        /// Check if the given context supports ray tracing.
        /// </summary>
        /// <returns>True if ray tracing is supported, otherwise false.</returns>
        MANDRILL_API bool supportsRayTracing() const
        {
            return mRayTracingSupport;
        }

        /// <summary>
        /// Get the current verical sync mode.
        /// </summary>
        /// <returns>True if vertical sync is activated, otherwise false.</returns>
        MANDRILL_API bool getVsync() const
        {
            return mVsync;
        }

        /// <summary>
        /// Set a vertical sync mode.
        /// </summary>
        /// <param name="vsync">True if vertical sync should be on, otherwise false.</param>
        /// <returns></returns>
        MANDRILL_API void setVsync(bool vsync)
        {
            mVsync = vsync;
        }

        /// <summary>
        /// Get the multisample anti-aliasing sample count.
        /// </summary>
        /// <returns>Sample count</returns>
        MANDRILL_API VkSampleCountFlagBits getSampleCount() const;

    private:
#if defined(_DEBUG)
        void createDebugMessenger();
#endif
        void createInstance();
        void createDevice(const std::vector<const char*>& extensions, uint32_t physicalDeviceIndex);
        void createCommandPool();
        void createSurface();
        void createExtensionProcAddrs();

        GLFWwindow* mpWindow;

#if defined(_DEBUG)
        VkDebugUtilsMessengerEXT mDebugMessenger;
#endif

        VkInstance mInstance;
        VkPhysicalDevice mPhysicalDevice;
        VkDevice mDevice;
        VkSurfaceKHR mSurface;

        DeviceProperties mProperties;

        uint32_t mQueueFamilyIndex;
        VkCommandPool mCommandPool;
        VkQueue mQueue;

        bool mRayTracingSupport;
        bool mVsync;
    };
} // namespace Mandrill
