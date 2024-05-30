#pragma once

#include "Common.h"

/// <summary>
/// Add more extensions to this file as needed. This will provide a function name that matches the API prototype. If a
/// quick and dirty way is enough, then opt for using the VK_LOAD or VK_CALL macros below.
///
/// New extensions are added in the extern "C" block below, in the Extension.cpp, and to
/// Device::createExtensionProcAddrs(). Don't forget to add a macro so the function can be called with the correct name.
///
/// </summary>

extern "C" {
extern MANDRILL_API PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_;
// Add more extensions here, don't forget the macro below.
}

// Define macros so we can use the same name as the API
#define vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR_

// Macro for loading a device function pointers as Xvk...()
#define VK_LOAD(device, func_name)                                                                                     \
    PFN_##func_name X##func_name = (PFN_##func_name)vkGetDeviceProcAddr(device, #func_name)

// Macro for calling a function via its vkGetDeviceProcAddr name
#define VK_CALL(device, func_name, ...)                                                                                \
    do {                                                                                                               \
        PFN_##func_name pfn_##func_name = (PFN_##func_name)vkGetDeviceProcAddr(device, #func_name);                    \
        pfn_##func_name(__VA_ARGS__);                                                                                  \
    } while (0);
