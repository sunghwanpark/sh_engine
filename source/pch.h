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

using namespace glm;

enum class rhi_type
{
    vulkan,
    dx12
};
