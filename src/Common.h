#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <source_location>
#include <string>
#include <unordered_map>
#include <vector>

#define MANDRILL_NAME "Mandrill"

// Version (set in CMakeLists.txt)
#ifndef MANDRILL_VERSION_MAJOR
#error "Missing major version number"
#endif
#ifndef MANDRILL_VERSION_MINOR
#error "Missing minor version number"
#endif
#ifndef MANDRILL_VERSION_PATCH
#error "Missing patch version number"
#endif

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
