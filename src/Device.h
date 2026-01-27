#pragma once

#include "Common.h"

namespace Mandrill
{
    // Forward declarations for factory methods
    class AccelerationStructure;
    class Buffer;
    class Camera;
    struct DescriptorDesc;
    class Descriptor;
    class Image;
    class Pass;
    struct PipelineDesc;
    class Pipeline;
    struct RayTracingPipelineDesc;
    class RayTracingPipeline;
    class Sampler;
    struct ShaderDesc;
    class Shader;
    class Scene;
    class Swapchain;
    enum class TextureType : uint32_t;
    class Texture;

    struct MANDRILL_API DeviceProperties {
        VkPhysicalDeviceProperties physicalDevice;
        VkPhysicalDeviceMemoryProperties memory;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipeline;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStucture;
    };

    /// <summary>
    /// Device class abstracting the physical and logical Vulkan device, as well as handling extensions and features.
    /// </summary>
    class Device : public std::enable_shared_from_this<Device>
    {
    public:
        MANDRILL_NON_COPYABLE(Device)

        /// <summary>
        /// Device that abstracts the physical and logical device, and their features and properties.
        /// </summary>
        /// <param name="pWindow">GLFW window to create render context for</param>
        /// <param name="extensions">Extra device extensions to activate</param>
        /// <param name="pFeatures">Pointer to a pNext-chain of features to link with when creating device, can be
        /// nullptr in which case a set of required default features will be used</param> <param
        /// name="physicalDeviceIndex">Physical device to use. Set this explicitly if device 0 is not the intended
        /// one.</param>
        MANDRILL_API Device(GLFWwindow* pWindow,
                            const std::vector<const char*>& extensions = std::vector<const char*>(),
                            VkPhysicalDeviceFeatures2* pFeatures = nullptr, uint32_t physicalDeviceIndex = 0);

        /// <summary>
        /// Destructor for device.
        /// </summary>
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
        MANDRILL_API void setVsync(bool vsync)
        {
            mVsync = vsync;
        }

        /// <summary>
        /// Get the multisample anti-aliasing sample count.
        /// </summary>
        /// <returns>Sample count</returns>
        MANDRILL_API VkSampleCountFlagBits getSampleCount() const;

        /// <summary>
        /// Create a new acceleration structure.
        /// </summary>
        /// <param name="wpScene">Scene to create the acceleration structure of</param>
        /// <param name="flags">Flags for building acceleration structure</param>
        MANDRILL_API ptr<AccelerationStructure> createAccelerationStructure(std::weak_ptr<Scene> wpScene,
                                                                            VkBuildAccelerationStructureFlagsKHR flags);

        /// <summary>
        /// Create a new buffer.
        /// </summary>
        /// <param name="size">Size of buffer in bytes</param>
        /// <param name="usage">How the buffer will be used</param>
        /// <param name="properties">What properties the memory should have</param>
        /// <returns>A new buffer</returns>
        MANDRILL_API ptr<Buffer> createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                              VkMemoryPropertyFlags properties);

        /// <summary>
        /// Create a new camera.
        /// </summary>
        /// <param name="pWindow">Window to use</param>
        /// <param name="pSwapchain">Swapchain to use</param>
        /// <returns>A new camera</returns>
        MANDRILL_API ptr<Camera> createCamera(GLFWwindow* pWindow, ptr<Swapchain> pSwapchain);

        /// <summary>
        /// Create a new descriptor.
        /// </summary>
        /// <param name="desc">Description of the descriptor being created</param>
        /// <param name="layout">Layout to use</param>
        /// <returns>A new descriptor</returns>
        MANDRILL_API ptr<Descriptor> createDescriptor(const std::vector<DescriptorDesc>& desc,
                                                      VkDescriptorSetLayout layout);

        /// <summary>
        /// Create a new Image and allocate memory for it.
        /// </summary>
        /// <param name="width">Width of image</param>
        /// <param name="height">Height of image</param>
        /// <param name="depth">Depth of image</param>
        /// <param name="mipLevels">Number of mipmapping levels</param>
        /// <param name="samples">Number of samples</param>
        /// <param name="format">Image format</param>
        /// <param name="tiling">Tiling mode to use</param>
        /// <param name="usage">How the image will be used</param>
        /// <param name="properties">Which memory properties to require</param>
        /// <returns>A new image</returns>
        MANDRILL_API ptr<Image> createImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
                                            VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                                            VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

        /// <summary>
        /// Create a new Image using memory that has already been allocated.
        /// </summary>
        /// <param name="width">Width of image</param>
        /// <param name="height">Height of image</param>
        /// <param name="depth">Depth of image</param>
        /// <param name="mipLevels">Number of mipmapping levels</param>
        /// <param name="samples">Number of samples</param>
        /// <param name="format">Image format</param>
        /// <param name="tiling">Tiling mode to use</param>
        /// <param name="usage">How the image will be used</param>
        /// <param name="memory">Allocated memory to use for image</param>
        /// <param name="offset">Where in the allocated memory the image should be stored</param>
        /// <returns>A new image</returns>
        MANDRILL_API ptr<Image> createImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
                                            VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                                            VkImageUsageFlags usage, VkDeviceMemory memory, VkDeviceSize offset);

        /// <summary>
        /// Create a pass with explicit color and depth attachments.
        /// </summary>
        /// <param name="colorAttachments">Vector of image to use as color attachments</param>
        /// <param name="pDepthAttachment">Depth attachment to use, can be nullptr</param>
        /// <returns>A new pass</returns>
        MANDRILL_API ptr<Pass> createPass(std::vector<ptr<Image>> colorAttachments, ptr<Image> pDepthAttachment);

        /// <summary>
        /// Create a pass with implicit attachments, given a certain extent and format. Same format will be used for all
        /// color attachments.
        /// </summary>
        /// <param name="extent">Resolution of attachments</param>
        /// <param name="format">Format of color attachment</param>
        /// <param name="colorAttachmentCount">Number of color attachments</param>
        /// <param name="depthAttachment">If depth attachment should be created</param>
        /// <param name="sampleCount">Multisampling count</param>
        /// <returns>A new pass</returns>
        MANDRILL_API ptr<Pass> createPass(VkExtent2D extent, VkFormat format, uint32_t colorAttachmentCount = 1,
                                          bool depthAttachment = true,
                                          VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

        /// <summary>
        /// Create a pass with implicit attachments, given a certain extent and format. The list of formats will create
        /// mathcing color attachments.
        /// </summary>
        /// <param name="extent">Resolution of attachments</param>
        /// <param name="formats">List of formats</param>
        /// <param name="depthAttachment">If depth attachment should be created</param>
        /// <param name="sampleCount">Multisampling count</param>
        /// <returns>A new pass</returns>
        MANDRILL_API ptr<Pass> createPass(VkExtent2D extent, std::vector<VkFormat> formats, bool depthAttachment = true,
                                          VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

        /// <summary>
        /// Create a new pipeline.
        /// </summary>
        /// <param name="pPass">Pass to use</param>
        /// <param name="pShader">Shader to use</param>
        /// <param name="desc">Description of pipeline</param>
        /// <returns>A new pipeline</returns>
        MANDRILL_API ptr<Pipeline> createPipeline(ptr<Pass> pPass, ptr<Shader> pShader, const PipelineDesc& desc);

        /// <summary>
        /// Create a new ray tracing pipeline.
        /// </summary>
        /// <param name="pShader">Shader to use</param>
        /// <param name="desc">Description of pipeline</param>
        /// <returns>A new ray-tracing pipeline</returns>
        MANDRILL_API ptr<RayTracingPipeline> createRayTracingPipeline(ptr<Shader> pShader,
                                                                      const RayTracingPipelineDesc& desc);

        /// <summary>
        /// Create a new scene.
        /// </summary>
        /// <returns>A new scene</returns>
        MANDRILL_API ptr<Scene> createScene();

        /// <summary>
        /// Create a new shader.
        /// </summary>
        /// <param name="desc">Description of shader being created</param>
        /// <returns>A new shader</returns>
        MANDRILL_API ptr<Shader> createShader(const std::vector<ShaderDesc>& desc);

        /// <summary>
        /// Create a new swapchain.
        /// </summary>
        /// <param name="framesInFlight">How many frames in flight that should be used</param>
        /// <returns>A new swapchain</returns>
        MANDRILL_API ptr<Swapchain> createSwapchain(uint32_t framesInFlight = 2);

        /// <summary>
        /// Create a new texture from a file.
        /// </summary>
        /// <param name="type">Type of texture</param>
        /// <param name="format">Format to use</param>
        /// <param name="path">Path to texture file</param>
        /// <param name="mipmaps">Whether to use mipmaps or not</param>
        /// <returns>A new texture</returns>
        MANDRILL_API ptr<Texture> createTextureFromFile(TextureType type, VkFormat format,
                                                        const std::filesystem::path& path, bool mipmaps = false);

        /// <summary>
        /// Create a new texture from a buffer.
        /// </summary>
        /// <param name="type">Type of texture</param>
        /// <param name="format">Format to use</param>
        /// <param name="pData">Pointer to texture data</param>
        /// <param name="width">Width of texture</param>
        /// <param name="height">Height of texture</param>
        /// <param name="depth">Depth of texture</param>
        /// <param name="bytesPerPixel">Number bytes per pixel</param>
        /// <param name="mipmaps">Whether to use mipmaps or not</param>
        /// <returns>A new texture</returns>
        MANDRILL_API ptr<Texture> createTextureFromBuffer(TextureType type, VkFormat format, const void* pData,
                                                          uint32_t width, uint32_t height, uint32_t depth,
                                                          uint32_t bytesPerPixel, bool mipmaps = false);

        /// <summary>
        /// Create a texture from an existing image.
        /// </summary>
        /// <param name="pImage">Image to use</param>
        /// <param name="mipmaps">Whether to use mipmaps or not</param>
        /// <returns></returns>
        MANDRILL_API ptr<Texture> createTextureFromImage(ptr<Image> pImage, bool mipmaps = false);

    private:
#if defined(_DEBUG)
        void createDebugMessenger();
#endif
        void createInstance();
        void createDevice(const std::vector<const char*>& extensions, VkPhysicalDeviceFeatures2* pFeatures,
                          uint32_t physicalDeviceIndex);
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
