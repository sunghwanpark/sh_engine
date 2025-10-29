#pragma once

#include <iostream>
#include <cstdint>
#include <vector>
#include <set>
#include <string>
#include <array>
#include <map>
#include <format>
#include <optional>
#include <algorithm>
#include <ranges>
#include <type_traits>
#include <utility>
#include <filesystem>
#include <fstream>
#include <cstddef>
#include <volk.h>
#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL

#include <gtx/string_cast.hpp>
#include <gtc/type_ptr.hpp>
#include <glfw3.h>
#include <glfw3native.h>
#include "enumHelper.h"
#include "util/hash.h"
#include <vk_mem_alloc.h>
#include "stb_image.h"
#include "meshoptimizer.h"

#if defined(_DEBUG)
#define ENABLE_AFTERMATH 0
#else
#define ENABLE_AFTERMATH 0
#endif

#ifdef ENABLE_AFTERMATH
#include "GFSDK_Aftermath_GpuCrashDump.h"
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"
#endif

#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")

#define DISABLE_OIT 1
#define MESHLET 1

using namespace glm;

enum class rhi_type
{
    vulkan,
    dx12
};

namespace
{
    static std::string now_stamp()
    {
        auto zt = std::chrono::zoned_time{ std::chrono::current_zone(), std::chrono::system_clock::now() };
        return std::format("{:%Y%m%d_%H%M%S}", zt);
    }

    static void save_binary(const std::filesystem::path& p, const void* data, uint32_t sz)
    {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream f(p, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data), sz);
    }
    static void save_text(const std::filesystem::path& p, const std::string_view s)
    {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream f(p);
        f << s;
    }
}

#define ASSERT(expr, ...) \
    do { \
        if (!(expr)) { \
            std::fprintf(stderr, "[ASSERT] %s:%d: %s\n", \
                         __FILE__, __LINE__, #expr); \
            std::fflush(stderr); \
            std::abort(); \
        } \
    } while (0)

#define ASSERTF(expr, fmt, ...) \
    do { \
        if (!(expr)) { \
            std::fprintf(stderr, "[ASSERT] %s:%d: %s: " fmt "\n", \
                         __FILE__, __LINE__, #expr, ##__VA_ARGS__); \
            std::fflush(stderr); \
            std::abort(); \
        } \
    } while (0)