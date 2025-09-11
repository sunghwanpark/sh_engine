cmake_minimum_required(VERSION 3.20)
project(VulkanApp)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Vulkan 찾기
find_package(Vulkan REQUIRED)

include(FetchContent)

# cgltf
FetchContent_Declare(
  cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(cgltf)

# Win32 API 사용을 위해 Windows 헤더 포함
if (WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

set(SRC_DIR ${CMAKE_SOURCE_DIR}/source)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp ${SRC_DIR}/*.h)

list(FILTER SOURCES EXCLUDE REGEX ".*/external/.*")

add_executable(VulkanApp 
    source/main.cpp
    source/pch.h
    source/enumHelper.h
    ${SOURCES}
)

message(STATUS "소스 경로: ${CMAKE_SOURCE_DIR}")
message(STATUS "현재 소스 경로: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "현재 빌드 경로: ${CMAKE_CURRENT_BINARY_DIR}")

add_library(cgltf_interface INTERFACE)
add_library(volk STATIC  "${CMAKE_CURRENT_SOURCE_DIR}/source/external/volk/volk.c")

# Vulkan 라이브러리 링크
target_link_libraries(VulkanApp PRIVATE Vulkan::Vulkan)
target_link_libraries(VulkanApp PRIVATE cgltf_interface)
target_link_libraries(VulkanApp PRIVATE ${CMAKE_SOURCE_DIR}/lib/glfw/glfw3.lib)
target_link_libraries(VulkanApp PRIVATE volk)

target_include_directories(VulkanApp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(VulkanApp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_include_directories(VulkanApp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/external/glm")
target_include_directories(VulkanApp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/external/glfw")
target_include_directories(VulkanApp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/external/vma")
target_include_directories(VulkanApp PRIVATE "${CMAKE_SOURCE_DIR}/source/external/stb")
target_include_directories(VulkanApp PRIVATE "${CMAKE_SOURCE_DIR}/source/external/mikktspace")
target_include_directories(cgltf_interface INTERFACE ${cgltf_SOURCE_DIR})
target_include_directories(volk PUBLIC "$ENV{VULKAN_SDK}/Include")
target_include_directories(volk PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/source/external/volk")

source_group(TREE ${SRC_DIR} PREFIX "Source Files" FILES ${SOURCES})

# 전역 PCH 등록
target_precompile_headers(VulkanApp PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/source/pch.h>)
#target_precompile_headers(VulkanApp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source/pch.h")
target_compile_definitions(VulkanApp PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(VulkanApp PRIVATE GLM_FORCE_LEFT_HANDED)

# DXC
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SPIRV_DIR  "${CMAKE_BINARY_DIR}/shaders")
set(DXC_PATH   "${CMAKE_SOURCE_DIR}/thirdparty/dxc/bin/x64/dxc.exe" CACHE FILEPATH "Path to DXC compiler")
set(HLSL_INCLUDE_DIR "${SHADER_DIR}")

file(GLOB SHADERS CONFIGURE_DEPENDS "${SHADER_DIR}/*.hlsl")
file(GLOB SHADERS_ALL "${SHADER_DIR}/*.*")

set(SPV_OUTPUTS "")
foreach(SHADER ${SHADERS})
    message(STATUS "shader: ${SHADER}")
    get_filename_component(FILE_NAME "${SHADER}" NAME)  # sample.vs.hlsl
    get_filename_component(FILE_WE   "${SHADER}" NAME_WE)

    if (FILE_NAME MATCHES "\\.([a-zA-Z0-9_]+)\\.hlsl$")
        set(STAGE_SUFFIX "${CMAKE_MATCH_1}")  # vs / ps / cs ...
    else()
        message(FATAL_ERROR "Cannot parse shader stage from ${FILE_NAME}")
    endif()

    if (STAGE_SUFFIX STREQUAL "vs")
        set(TARGET_PROFILE "vs_6_0")
    elseif (STAGE_SUFFIX STREQUAL "ps")
        set(TARGET_PROFILE "ps_6_0")
    elseif (STAGE_SUFFIX STREQUAL "cs")
        set(TARGET_PROFILE "cs_6_0")
    else()
        message(FATAL_ERROR "Unknown shader stage '${STAGE_SUFFIX}' in ${FILE_NAME}")
    endif()

    set(SPIRV ${SPIRV_DIR}/${FILE_WE}.${STAGE_SUFFIX}.spv)
    message(STATUS "gen shader: ${SPIRV}")

    add_custom_command(
        OUTPUT ${SPIRV}
        DEPENDS ${SHADER}
        COMMAND ${DXC_PATH} 
                -spirv -fspv-target-env=vulkan1.2
                -fvk-use-dx-layout
                -fvk-support-nonzero-base-instance
                -fspv-extension=SPV_EXT_descriptor_indexing
                -T ${TARGET_PROFILE} 
                -E main
                -I "${HLSL_INCLUDE_DIR}"
                "${SHADER}" -Fo "${SPIRV}"
        COMMENT "Compiling ${SHADER} -> ${SPIRV}"
    )
    list(APPEND SPV_OUTPUTS ${SPIRV})
endforeach()
add_custom_target(CompileShaders DEPENDS ${SPV_OUTPUTS})
add_dependencies(VulkanApp CompileShaders)

target_sources(VulkanApp PRIVATE "${CMAKE_SOURCE_DIR}/source/external/mikktspace/mikktspace.c")
target_sources(VulkanApp PRIVATE ${SHADERS_ALL})
set_source_files_properties(${SHADERS_ALL} PROPERTIES HEADER_FILE_ONLY TRUE)

source_group(TREE "${SHADER_DIR}" PREFIX "Shader Files" FILES ${SHADERS_ALL})
