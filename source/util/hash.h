#pragma once

#include <glm.hpp>

inline glm::u64 fnv1a64(const void* data, glm::u32 size, glm::u64 hash = 1469598103934665603ull)
{
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data);
    while (size--)
    {
        hash ^= *ptr++;
        hash *= 1099511628211ull;
    }
    return hash;
}

template<typename T>
inline glm::u64 hash_combine(glm::u64 h, const T& v)
{
    return fnv1a64(&v, sizeof(T), h);
}

inline glm::u64 hash_combine(glm::u64 h, const std::string& s)
{
    return fnv1a64(s.data(), static_cast<glm::u32>(s.size()), h);
}
