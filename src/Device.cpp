#include "Device.h"

#include "AccelerationStructure.h"
#include "Buffer.h"
#include "Descriptor.h"
#include "Error.h"
#include "Extension.h"
#include "Image.h"
#include "Log.h"
#include "Pass.h"
#include "Pipeline.h"
#include "RayTracingPipeline.h"
#include "Sampler.h"
#include "Scene.h"
#include "Shader.h"
#include "Texture.h"

#if MANDRILL_LINUX
#include <csignal>
#endif

using namespace Mandrill;

#if defined(_DEBUG)
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData)
{
    // clang-format off
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        Log::Warning("{}: {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        Log::Error("{}: {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);
#if MANDRILL_WINDOWS 
        __debugbreak();         // Vulkan encountered an error and since you are running Mandrill
#elif MANDRILL_LINUX            // in debug mode, the execution is halted. You can inspect the log,
        std::raise(SIGTRAP);    // and the call stack in your debugger to find out where in the code
#endif                          // the error was encountered.
        break; 
    }
    // clang-format on

    return VK_FALSE;
}

static VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                             const VkAllocationCallbacks* pAllocator,
                                             VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                          const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& ci)
{
    ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
    };
}

void Device::createDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT ci;
    populateDebugMessengerCreateInfo(ci);

    Check::Vk(createDebugUtilsMessengerEXT(mInstance, &ci, nullptr, &mDebugMessenger));
}

#endif

Device::Device(GLFWwindow* pWindow, const std::vector<const char*>& extensions, VkPhysicalDeviceFeatures2* pFeatures,
               uint32_t physicalDeviceIndex)
    : mpWindow(pWindow), mVsync(true), mRayTracingSupport(false)
{
    createInstance();

#if defined(_DEBUG)
    createDebugMessenger();
#endif

    createSurface();
    createDevice(extensions, pFeatures, physicalDeviceIndex);
    createCommandPool();
    createExtensionProcAddrs();
}

Device::~Device()
{
    if (mCommandPool) {
        vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    }
    if (mDevice) {
        vkDestroyDevice(mDevice, nullptr);
    }
    if (mSurface) {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    }

#if defined(_DEBUG)
    destroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
#endif

    if (mInstance) {
        vkDestroyInstance(mInstance, nullptr);
    }
}

VkSampleCountFlagBits Device::getSampleCount() const
{
    VkSampleCountFlags counts = mProperties.physicalDevice.limits.framebufferColorSampleCounts &
                                mProperties.physicalDevice.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT) {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT) {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT) {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT) {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT) {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}


void Device::createInstance()
{
    VkApplicationInfo ai = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Mandrill App",
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = MANDRILL_NAME,
        .engineVersion = VK_MAKE_API_VERSION(0, MANDRILL_VERSION_MAJOR, MANDRILL_VERSION_MINOR, MANDRILL_VERSION_PATCH),
        .apiVersion = VK_API_VERSION_1_3,
    };

    uint32_t n;
    glfwGetRequiredInstanceExtensions(&n);
    const char** tmp = glfwGetRequiredInstanceExtensions(&n);

    if (!tmp) {
        Log::Error("No Vulkan instance extensions found for GLFW.");
        Check::GLFW();
    }

    std::vector<const char*> extensions(tmp, tmp + n);

    Log::Debug("GLFW required instance extensions ({}):", n);
    for (auto& e : extensions) {
        Log::Debug(" * {}", e);
    }

    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &ai,
    };

#if defined(_DEBUG)
    // Use validation layers
    std::vector<const char*> layers = {"VK_LAYER_KHRONOS_validation"};

    Check::Vk(vkEnumerateInstanceLayerProperties(&n, nullptr));
    std::vector<VkLayerProperties> props(n);
    Check::Vk(vkEnumerateInstanceLayerProperties(&n, props.data()));

    Log::Debug("Available layers ({}):", count(props));
    for (auto& p : props) {
        Log::Debug(" * {}", p.layerName);
    }

    if (std::find_if(props.begin(), props.end(), [layers](const VkLayerProperties& p) {
            return std::string(p.layerName) == std::string(layers[0]);
        }) == props.end()) {
        Log::Error("Validation layer not supported");
    }

    ci.enabledLayerCount = count(layers);
    ci.ppEnabledLayerNames = layers.data();

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    ci.enabledExtensionCount = count(extensions);
    ci.ppEnabledExtensionNames = extensions.data();

    Check::Vk(vkCreateInstance(&ci, nullptr, &mInstance));

    uint32_t version;
    Check::Vk(vkEnumerateInstanceVersion(&version));
    Log::Info("Created Vulkan instance: {}.{}.{}", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version),
              VK_API_VERSION_PATCH(version));
}

// Check if device supports requested extensions
static bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice, std::vector<const char*> requestedExtensions,
                                        bool print = false)
{
    uint32_t n;
    Check::Vk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &n, nullptr));
    std::vector<VkExtensionProperties> availableExtensions(n);
    Check::Vk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &n, availableExtensions.data()));

    if (print) {
        Log::Info("Requesting device extensions ({}):", count(requestedExtensions));
        for (auto& e : requestedExtensions) {
            Log::Info(" * {}", e);
        }

        Log::Debug("Available device extensions ({}):", n);
        for (auto& e : availableExtensions) {
            Log::Debug(" * {}", e.extensionName);
        }
    }

    bool result = true;
    for (auto& e : requestedExtensions) {
        bool found = false;

        if (std::find_if(availableExtensions.begin(), availableExtensions.end(), [e](const VkExtensionProperties& p) {
                return std::string(p.extensionName) == std::string(e);
            }) != availableExtensions.end()) {
            found = true;
        }

        if (!found) {
            if (print) {
                Log::Error("The requested extension {} is not available", e);
            }
            result = false;
        }
    }

    return result;
}

static uint32_t getQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                    VkQueueFlags requiredFamilyFlags)
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queueFamilyProperties.data());

    if (!count) {
        Log::Error("No Vulkan queue family available");
    }

    Log::Debug("Available queue families for selected device: {}", count);

    uint32_t index = 0;
    for (auto& p : queueFamilyProperties) {
        if (p.queueFlags & requiredFamilyFlags) {
            break;
        }
        index++;
    }

    if (index == count) {
        Log::Error("No Vulkan queue found for requested families");
    }

    // Check that the selected queue family supports PRESENT
    VkBool32 supported;
    Check::Vk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, surface, &supported));
    if (!supported) {
        Log::Error("Selected queue family does not support PRESENT");
    }

    return index;
}

void Device::createDevice(const std::vector<const char*>& extensions, VkPhysicalDeviceFeatures2* pFeatures,
                          uint32_t physicalDeviceIndex)
{
    std::array baseExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    std::vector<const char*> raytracingExtensions = {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    };

    std::vector<const char*> deviceExtensions;
    deviceExtensions.insert(deviceExtensions.end(), baseExtensions.begin(), baseExtensions.end());
    deviceExtensions.insert(deviceExtensions.end(), extensions.begin(), extensions.end());

    // Iterate all physical devices
    uint32_t n;
    Check::Vk(vkEnumeratePhysicalDevices(mInstance, &n, nullptr));
    std::vector<VkPhysicalDevice> physicalDevices(n);
    Check::Vk(vkEnumeratePhysicalDevices(mInstance, &n, physicalDevices.data()));

    Log::Info("Available devices ({}):", n);
    for (uint32_t i = 0; i < count(physicalDevices); i++) {
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtp = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        };

        VkPhysicalDeviceAccelerationStructurePropertiesKHR asp = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            .pNext = &rtp,
        };

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBuffer = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
            .pNext = &asp,
        };

        VkPhysicalDeviceDriverProperties driver = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
            .pNext = &descriptorBuffer,
        };

        VkPhysicalDeviceProperties2 prop = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &driver,
        };

        vkGetPhysicalDeviceProperties2(physicalDevices[i], &prop);

        if (i == physicalDeviceIndex) {
            mPhysicalDevice = physicalDevices[i];
            vkGetPhysicalDeviceProperties(mPhysicalDevice, &mProperties.physicalDevice);
            vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mProperties.memory);
            mProperties.rayTracingPipeline = rtp;
            mProperties.accelerationStucture = asp;
        }

        Log::Info(" * [{}] {}, driver: {} {}, Vulkan {}.{}.{} {}", i, prop.properties.deviceName, driver.driverName,
                  driver.driverInfo, VK_API_VERSION_MAJOR(prop.properties.apiVersion),
                  VK_API_VERSION_MINOR(prop.properties.apiVersion), VK_API_VERSION_PATCH(prop.properties.apiVersion),
                  i == physicalDeviceIndex ? "(chosen)" : "");
    }

    // Check for ray tracing support
    bool print = false;
    mRayTracingSupport = checkDeviceExtensionSupport(mPhysicalDevice, raytracingExtensions, print);

    if (mRayTracingSupport) {
        deviceExtensions.insert(deviceExtensions.end(), raytracingExtensions.begin(), raytracingExtensions.end());
    } else {
        Log::Warning("The chosen physical device does not support ray tracing");
    }

    // Check for extension support
    print = true;
    if (!checkDeviceExtensionSupport(mPhysicalDevice, deviceExtensions, print)) {
        Log::Error("The chosen physical device does not support the requested extensions");
        return;
    }

    mQueueFamilyIndex = getQueueFamilyIndex(mPhysicalDevice, mSurface,
                                            VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = mQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    // Default features
    VkPhysicalDeviceFeatures2 features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .features =
            {
                .fillModeNonSolid = VK_TRUE,
                .wideLines = VK_TRUE,
                .samplerAnisotropy = VK_TRUE,
                .vertexPipelineStoresAndAtomics = VK_TRUE,
                .fragmentStoresAndAtomics = VK_TRUE,
                .shaderInt64 = VK_TRUE,
            },
    };

    // Ray-tracing features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .rayTracingPipeline = VK_TRUE,
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &rtFeatures,
        .accelerationStructure = VK_TRUE,
    };

    VkPhysicalDeviceVulkan11Features vk11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        // Hook on the ray tracing features
        .pNext = mRayTracingSupport ? &asFeature : nullptr,
    };

    VkPhysicalDeviceVulkan12Features vk12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vk11Features,
        .uniformAndStorageBuffer8BitAccess = VK_TRUE,
        .descriptorIndexing = mRayTracingSupport,
        .timelineSemaphore = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
        .vulkanMemoryModel = VK_TRUE,
        .vulkanMemoryModelDeviceScope = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features vk13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vk12Features,
        .shaderDemoteToHelperInvocation = VK_TRUE,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan14Features vk14Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = &vk13Features,
        .dynamicRenderingLocalRead = VK_TRUE,
        .pushDescriptor = VK_TRUE,
    };

    features2.pNext = &vk14Features;

    VkDeviceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = pFeatures ? pFeatures : &features2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = count(deviceExtensions),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };

    Check::Vk(vkCreateDevice(mPhysicalDevice, &ci, nullptr, &mDevice));
}

void Device::createCommandPool()
{
    VkCommandPoolCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = mQueueFamilyIndex,
    };

    Check::Vk(vkCreateCommandPool(mDevice, &ci, nullptr, &mCommandPool));
    vkGetDeviceQueue(mDevice, mQueueFamilyIndex, 0, &mQueue);
}

void Device::createSurface()
{
    Check::Vk(glfwCreateWindowSurface(mInstance, mpWindow, nullptr, &mSurface));
}

void Device::createExtensionProcAddrs()
{
    vkCmdPushDescriptorSetKHR =
        reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkGetDeviceProcAddr(mDevice, "vkCmdPushDescriptorSetKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(mDevice, "vkCreateRayTracingPipelinesKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(mDevice, "vkCmdBuildAccelerationStructuresKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(mDevice, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(mDevice, "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(mDevice, "vkGetAccelerationStructureBuildSizesKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(mDevice, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(mDevice, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(mDevice, "vkCmdTraceRaysKHR"));
    vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetDeviceProcAddr(mDevice, "vkSetDebugUtilsObjectNameEXT"));
}

ptr<AccelerationStructure> Device::createAccelerationStructure(std::weak_ptr<Scene> wpScene,
                                                               VkBuildAccelerationStructureFlagsKHR flags)
{
    return make_ptr<AccelerationStructure>(shared_from_this(), wpScene, flags);
}

ptr<Buffer> Device::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    return make_ptr<Buffer>(shared_from_this(), size, usage, properties);
}

ptr<Camera> Device::createCamera(GLFWwindow* pWindow, ptr<Swapchain> pSwapchain)
{
    return make_ptr<Camera>(shared_from_this(), pWindow, pSwapchain);
}

ptr<Descriptor> Device::createDescriptor(const std::vector<DescriptorDesc>& desc, VkDescriptorSetLayout layout)
{
    return make_ptr<Descriptor>(shared_from_this(), desc, layout);
}

ptr<Image> Device::createImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
                               VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                               VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
    return make_ptr<Image>(shared_from_this(), width, height, depth, mipLevels, samples, format, tiling, usage,
                           properties);
}

ptr<Image> Device::createImage(uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels,
                               VkSampleCountFlagBits samples, VkFormat format, VkImageTiling tiling,
                               VkImageUsageFlags usage, VkDeviceMemory memory, VkDeviceSize offset)
{
    return make_ptr<Image>(shared_from_this(), width, height, depth, mipLevels, samples, format, tiling, usage, memory,
                           offset);
}

ptr<Pass> Device::createPass(std::vector<ptr<Image>> colorAttachments, ptr<Image> pDepthAttachment)
{
    return make_ptr<Pass>(shared_from_this(), colorAttachments, pDepthAttachment);
}

ptr<Pass> Device::createPass(VkExtent2D extent, VkFormat format, uint32_t colorAttachmentCount, bool depthAttachment,
                             VkSampleCountFlagBits sampleCount)
{
    return make_ptr<Pass>(shared_from_this(), extent, format, colorAttachmentCount, depthAttachment, sampleCount);
}

ptr<Pass> Device::createPass(VkExtent2D extent, std::vector<VkFormat> formats, bool depthAttachment,
                             VkSampleCountFlagBits sampleCount)
{
    return make_ptr<Pass>(shared_from_this(), extent, formats, depthAttachment, sampleCount);
}

ptr<Pipeline> Device::createPipeline(ptr<Pass> pPass, ptr<Shader> pShader, const PipelineDesc& desc)
{
    return make_ptr<Pipeline>(shared_from_this(), pPass, pShader, desc);
}

ptr<RayTracingPipeline> Device::createRayTracingPipeline(ptr<Shader> pShader,
                                                         const RayTracingPipelineDesc& desc)
{
    return make_ptr<RayTracingPipeline>(shared_from_this(), pShader, desc);
}

ptr<Sampler> Device::createSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerMipmapMode mipmapMode,
                                   VkSamplerAddressMode addressModeU, VkSamplerAddressMode addressModeV,
                                   VkSamplerAddressMode addressModeW)
{
    return make_ptr<Sampler>(shared_from_this(), magFilter, minFilter, mipmapMode, addressModeU, addressModeV,
                             addressModeW);
}

ptr<Texture> Device::createTexture(TextureType type, VkFormat format, const std::filesystem::path& path, bool mipmaps)
{
    return make_ptr<Texture>(shared_from_this(), type, format, path, mipmaps);
}

ptr<Scene> Device::createScene()
{
    return make_ptr<Scene>(shared_from_this());
}

ptr<Shader> Device::createShader(const std::vector<ShaderDesc>& desc)
{
    return make_ptr<Shader>(shared_from_this(), desc);
}

ptr<Swapchain> Device::createSwapchain(uint32_t framesInFlight)
{
    return make_ptr<Swapchain>(shared_from_this(), framesInFlight);
}

ptr<Texture> Device::createTexture(TextureType type, VkFormat format, const void* pData, uint32_t width,
                                   uint32_t height, uint32_t depth, uint32_t channels, bool mipmaps)
{
    return make_ptr<Texture>(shared_from_this(), type, format, pData, width, height, depth, channels, mipmaps);
}
