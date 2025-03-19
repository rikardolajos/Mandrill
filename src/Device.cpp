#include "Device.h"

#include "Error.h"
#include "Extension.h"
#include "Log.h"

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
    switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        Log::warning("{}: {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        Log::error("{}: {}", pCallbackData->pMessageIdName, pCallbackData->pMessage);
#if MANDRILL_WINDOWS
        __debugbreak();         // Vulkan encountered an error and since you are running Mandrill
#elif MANDRILL_LINUX            // in debug mode, the execution is halted. You can inspect the log,
        std::raise(SIGTRAP);    // and the call stack in your debugger to find out where in the code
#endif                          // the error was encountered.
        break;
    }

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

Device::Device(GLFWwindow* pWindow, const std::vector<const char*>& extensions, uint32_t physicalDeviceIndex)
    : mpWindow(pWindow), mVsync(true), mRayTracingSupport(false)
{
    createInstance();

#if defined(_DEBUG)
    createDebugMessenger();
#endif

    createSurface();
    createDevice(extensions, physicalDeviceIndex);
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
        Log::error("No Vulkan instance extensions found for GLFW.");
        Check::GLFW();
    }

    std::vector<const char*> extensions(tmp, tmp + n);

    Log::debug("GLFW required instance extensions ({}):", n);
    for (auto& e : extensions) {
        Log::debug(" * {}", e);
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

    Log::debug("Available layers ({}):", count(props));
    for (auto& p : props) {
        Log::debug(" * {}", p.layerName);
    }

    if (std::find_if(props.begin(), props.end(), [layers](const VkLayerProperties& p) {
            return std::string(p.layerName) == std::string(layers[0]);
        }) == props.end()) {
        Log::error("Validation layer not supported");
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
    Log::info("Created Vulkan instance: {}.{}.{}", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version),
              VK_API_VERSION_PATCH(version));
}

// Check if device supports requested extensions
static bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice, std::vector<const char*> requestedExtensions)
{
    uint32_t n;
    Check::Vk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &n, nullptr));
    std::vector<VkExtensionProperties> availableExtensions(n);
    Check::Vk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &n, availableExtensions.data()));

    Log::info("Requesting device extensions ({}):", count(requestedExtensions));
    for (auto& e : requestedExtensions) {
        Log::info(" * {}", e);
    }

    Log::debug("Available device extensions ({}):", n);
    for (auto& e : availableExtensions) {
        Log::debug(" * {}", e.extensionName);
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
            Log::error("The requested extension {} is not available", e);
            result = false;
        }
    }

    return result;
}

static uint32_t getQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, uint32_t requiredFamilyFlags)
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queueFamilyProperties.data());

    if (!count) {
        Log::error("No Vulkan queue family available");
    }

    Log::debug("Available queue families for selected device: {}", count);

    uint32_t index = 0;
    for (auto& p : queueFamilyProperties) {
        if (p.queueFlags & requiredFamilyFlags) {
            break;
        }
        index++;
    }

    if (index == count) {
        Log::error("No Vulkan queue found for requested families");
    }

    // Check that the selected queue family supports PRESENT
    VkBool32 supported;
    Check::Vk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, surface, &supported));
    if (!supported) {
        Log::error("Selected queue family does not support PRESENT");
    }

    return index;
}

void Device::createDevice(const std::vector<const char*>& extensions, uint32_t physicalDeviceIndex)
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

    Log::info("Available devices ({}):", n);
    for (uint32_t i = 0; i < count(physicalDevices); i++) {
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtp = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        };

        VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBuffer = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
            .pNext = &rtp,
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
        }

        Log::info(" * {}, driver: {} {}, Vulkan {}.{}.{} {}", prop.properties.deviceName, driver.driverName,
                  driver.driverInfo, VK_API_VERSION_MAJOR(prop.properties.apiVersion),
                  VK_API_VERSION_MINOR(prop.properties.apiVersion), VK_API_VERSION_PATCH(prop.properties.apiVersion),
                  i == physicalDeviceIndex ? "(chosen)" : "");
    }

    // Check for ray tracing support
    mRayTracingSupport = checkDeviceExtensionSupport(mPhysicalDevice, raytracingExtensions);

    if (mRayTracingSupport) {
        deviceExtensions.insert(deviceExtensions.end(), raytracingExtensions.begin(), raytracingExtensions.end());
    } else {
        Log::warning("The chosen physical device does not support ray tracing");
    }

    // Check for extension support
    if (!checkDeviceExtensionSupport(mPhysicalDevice, deviceExtensions)) {
        Log::error("The chosen physical device does not support the requested extensions");
        return;
    }

    mQueueFamilyIndex = getQueueFamilyIndex(mPhysicalDevice, mSurface,
                                            VK_QUEUE_GRAPHICS_BIT && VK_QUEUE_COMPUTE_BIT && VK_QUEUE_TRANSFER_BIT);

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
                .shaderStorageImageWriteWithoutFormat = VK_TRUE,
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
        .descriptorIndexing = mRayTracingSupport,
        .timelineSemaphore = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
        .vulkanMemoryModel = VK_TRUE,
        .vulkanMemoryModelDeviceScope = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features vk13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vk12Features,
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
        .pNext = &features2,
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
