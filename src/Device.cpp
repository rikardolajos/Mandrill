#include "Device.h"

#include "Error.h"
#include "Extension.h"
#include "Log.h"

using namespace Mandrill;

Device::Device(GLFWwindow* pWindow, const std::vector<const char*>& extensions, uint32_t physicalDeviceIndex)
    : mpWindow(pWindow)
{
    createInstance();
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

    uint32_t count;
    glfwGetRequiredInstanceExtensions(&count);
    const char** tmp = glfwGetRequiredInstanceExtensions(&count);

    if (!tmp) {
        Log::error("No Vulkan instance extensions found for GLFW.");
        Check::GLFW();
    }

    std::vector<const char*> extensions(tmp, tmp + count);

    Log::debug("GLFW required instance extensions ({}):", count);
    for (auto& e : extensions) {
        Log::debug(" * {}", e);
    }

    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &ai,
        .enabledExtensionCount = count,
        .ppEnabledExtensionNames = extensions.data(),
    };

#if defined(_DEBUG)
    // Use validation layers
    std::vector<const char*> layers = {"VK_LAYER_KHRONOS_validation"};

    Check::Vk(vkEnumerateInstanceLayerProperties(&count, nullptr));
    std::vector<VkLayerProperties> props(count);
    Check::Vk(vkEnumerateInstanceLayerProperties(&count, props.data()));

    Log::debug("Available layers ({}):", props.size());
    for (auto& p : props) {
        Log::debug(" * {}", p.layerName);
    }

    if (std::find_if(props.begin(), props.end(), [layers](const VkLayerProperties& p) {
            return std::string(p.layerName) == std::string(layers[0]);
        }) == props.end()) {
        Log::error("Validation layer not supported.");
    }

    ci.enabledLayerCount = static_cast<uint32_t>(layers.size());
    ci.ppEnabledLayerNames = layers.data();
#endif

    Check::Vk(vkCreateInstance(&ci, nullptr, &mInstance));

    uint32_t version;
    Check::Vk(vkEnumerateInstanceVersion(&version));
    Log::info("Created Vulkan instance: {}.{}.{}", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version),
              VK_API_VERSION_PATCH(version));
}

// Check if device supports requested extensions
static bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice, std::vector<const char*> requestedExtensions)
{
    uint32_t count;
    Check::Vk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr));
    std::vector<VkExtensionProperties> availableExtensions(count);
    Check::Vk(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, availableExtensions.data()));

    Log::info("Requesting device extensions ({}):", requestedExtensions.size());
    for (auto& e : requestedExtensions) {
        Log::info(" * {}", e);
    }

    Log::debug("Available device extensions ({}):", count);
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
            Log::error("The requested extension {} is not available.", e);
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
        Log::error("No Vulkan queue family available.");
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
        Log::error("No Vulkan queue found for requested families.");
    }

    // Check that the selected queue family supports PRESENT
    VkBool32 supported;
    Check::Vk(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, surface, &supported));
    if (!supported) {
        Log::error("Selected queue family does not support PRESENT.");
    }

    return index;
}

void Device::createDevice(const std::vector<const char*>& extensions, uint32_t physicalDeviceIndex)
{
    std::array baseExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        // VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    };

    std::vector<const char*> raytracingExtensions = {
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    };

    std::vector<const char*> deviceExtensions;
    deviceExtensions.insert(deviceExtensions.end(), baseExtensions.begin(), baseExtensions.end());
    deviceExtensions.insert(deviceExtensions.end(), extensions.begin(), extensions.end());

    // Iterate all physical devices
    uint32_t count;
    Check::Vk(vkEnumeratePhysicalDevices(mInstance, &count, nullptr));
    std::vector<VkPhysicalDevice> physicalDevices(count);
    Check::Vk(vkEnumeratePhysicalDevices(mInstance, &count, physicalDevices.data()));

    Log::info("Available devices ({}):", count);
    for (int i = 0; i < physicalDevices.size(); i++) {
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
        Log::warning("The chosen physical device does not support ray tracing.");
    }

    // Check for extension support
    if (!checkDeviceExtensionSupport(mPhysicalDevice, deviceExtensions)) {
        Log::error("The chosen physical device does not support the requested extensions.");
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
                .samplerAnisotropy = VK_TRUE,
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
        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features vk13Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vk12Features,
        .synchronization2 = VK_TRUE,
    };

    features2.pNext = &vk13Features;

    VkDeviceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
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
}
