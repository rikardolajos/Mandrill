#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#define MANDRILL_NAME "Mandrill"

// Version
#define MANDRILL_VERSION_MAJOR 2024
#define MANDRILL_VERSION_MINOR 1
#define MANDRILL_VERSION_PATCH 1

// Platform
#define MANDRILL_PLATFORM_WINDOWS 1
#define MANDRILL_PLATFORM_LINUX 2

#ifndef MANDRILL_PLATFORM
#if defined(_WIN64)
#define MANDRILL_PLATFORM MANDRILL_PLATFORM_WINDOWS
#elif defined(__linux__)
#define MANDRILL_PLATFORM MANDRILL_PLATFORM_LINUX
#else
#error "Unsupported target platform"
#endif
#endif

#define MANDRILL_WINDOWS (MANDRILL_PLATFORM == MANDRILL_PLATFORM_WINDOWS)
#define MANDRILL_LINUX (MANDRILL_PLATFORM == MANDRILL_PLATFORM_LINUX)

// Library export
#if MANDRILL_WINDOWS
#define MANDRILL_API_EXPORT __declspec(dllexport)
#define MANDRILL_API_IMPORT __declspec(dllimport)
#elif MANDRILL_LINUX
#define MANDRILL_API_EXPORT __attribute__((visibility("default")))
#define MANDRILL_API_IMPORT
#endif

#ifdef MANDRILL_DLL
#define MANDRILL_API MANDRILL_API_EXPORT
#else
#define MANDRILL_API MANDRILL_API_IMPORT
#endif

// Macro for loading a device function pointers as Xvk...()
#define VK_LOAD(device, func_name)                                                                                     \
    PFN_##func_name X##func_name = (PFN_##func_name)vkGetDeviceProcAddr(device, #func_name)

// Macro for calling a function via its vkGetDeviceProcAddr name
#define VK_CALL(device, func_name, ...)                                                                                \
    do {                                                                                                               \
        PFN_##func_name pfn_##func_name = (PFN_##func_name)vkGetDeviceProcAddr(device, #func_name);                    \
        pfn_##func_name(__VA_ARGS__);                                                                                  \
    } while (0);
