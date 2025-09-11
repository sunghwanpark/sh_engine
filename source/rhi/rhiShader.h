#pragma once

#include "pch.h"

struct rhiShaderBinary 
{
    const void* data = nullptr;
    u32 size = 0;
    std::string entry = "main";
};

namespace shaderio
{
    inline rhiShaderBinary load_shader_binary(const std::filesystem::path& path, std::string_view entry = "main")
    {
        auto read = [](const std::filesystem::path& path) -> std::vector<u8>
            {
                std::ifstream f(path, std::ios::binary | std::ios::ate);
                if (!f)
                    throw std::runtime_error("Failed to open file: " + path.string());

                const std::streamsize size = f.tellg();
                if (size <= 0)
                    throw std::runtime_error("File is empty: " + path.string());

                std::vector<u8> data(static_cast<u32>(size));
                f.seekg(0, std::ios::beg);
                if (!f.read(reinterpret_cast<char*>(data.data()), size))
                    throw std::runtime_error("Failed to read file: " + path.string());

                return data;
            };

        auto to_lower = [](std::string s)
            {
                std::ranges::transform(s, s.begin(), [](unsigned char c)
                    {
                        return static_cast<char>(std::tolower(c));
                    });
                return s;
            };

        auto blob = read(path);

        const auto ext = to_lower(path.extension().string());
        if (ext == ".spv" && (blob.size() % 4u) != 0u) {
            throw std::runtime_error("SPIR-V size must be multiple of 4: " + path.string());
        }

        auto* mem = new uint8_t[blob.size()];
        std::memcpy(mem, blob.data(), blob.size());

        rhiShaderBinary bin;
        bin.data = mem;
        bin.size = static_cast<u32>(blob.size());
        bin.entry = std::move(entry);
        return bin;
    }

    inline void free_shader_binary(rhiShaderBinary& bin)
    {
        if (bin.data) 
        {
            delete[] reinterpret_cast<const uint8_t*>(bin.data);
            bin.data = nullptr;
            bin.size = 0;
        }
    }
}