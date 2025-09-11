#pragma once

#include "pch.h"

class glTFMesh;

constexpr u64 string_to_u64hash(std::string_view s) noexcept
{
    // 64-bit FNV-1a
    u64 hash = 14695981039346656037ull;   // offset basis
    constexpr u64 prime = 1099511628211ull; // FNV prime

    for (unsigned char c : s)
    {
        hash ^= static_cast<u64>(c);
        hash *= prime;
    }
    return hash;
}

class meshModelManager
{
public:
    std::weak_ptr<glTFMesh> get_asset(std::string_view asset_path);
    std::weak_ptr<glTFMesh> get_or_create_asset(std::string_view asset_path);

private:
    std::weak_ptr<glTFMesh> create_asset(std::string_view asset_path);

private:
    std::unordered_map<u64, std::shared_ptr<glTFMesh>> asset_cache;
};