#pragma once

#include "pch.h"

// ===== pack 10:10:10:2 SNORM (A2B10G10R10_SNORM_PACK32) =====
static inline i32 quantize_snorm10(f32 v) 
{
    f32 c = std::max(-1.0f, std::min(1.0f, v));
    i32 q = static_cast<i32>(std::lrintf(c * 511.0f));      // [-512, 511]
    q = std::max(-512, std::min(511, q));
    return q & 0x3FF;
}

static inline u32 pack_1010102_snorm(f32 x, f32 y, f32 z, i32 a)
{
    u32 rx = static_cast<u32>(quantize_snorm10(x));
    u32 ry = static_cast<u32>(quantize_snorm10(y));
    u32 rz = static_cast<u32>(quantize_snorm10(z));
    u32 ra = static_cast<u32>((a & 0x3));
    return (rx) | (ry << 10) | (rz << 20) | (ra << 30);
}

// ===== handedness = sign(dot(cross(N,T), B)) -> {0,1} =====
static inline int tangent_handedness(const std::array<f32, 3>& N, const std::array<f32, 3>& T, const std::array<f32, 3>& B)
{
    const f32 cx = N[1] * T[2] - N[2] * T[1];
    const f32 cy = N[2] * T[0] - N[0] * T[2];
    const f32 cz = N[0] * T[1] - N[1] * T[0];
    const f32 d = cx * B[0] + cy * B[1] + cz * B[2];
    return (d < 0.0f) ? 0 : 1;
}

// ===== float -> half (IEEE 754 binary16) + packHalf2x16 =====
static inline uint16_t float_to_half(f32 f)
{
    union 
    { 
        u32 u; 
        f32 f; 
    } v = { .f = f };

    u32 x = v.u;
    u32 sign = (x >> 31) & 0x1;
    i32  exp = static_cast<i32>((x >> 23) & 0xFF) - 127 + 15;
    u32 mant = x & 0x7FFFFF;

    if (exp <= 0) // subnormal/zero
    { 
        if (exp < -10) 
            return static_cast<u16>(sign << 15);
        mant |= 0x800000;
        u32 m = mant >> (1 - exp + 13);
        return static_cast<u16>((sign << 15) | m);
    }
    else if (exp >= 31) // inf/NaN
    { 
        return static_cast<u16>((sign << 15) | (0x1F << 10) | (mant ? 0x1 : 0));
    }
    u16 he = static_cast<u16>(exp);
    u16 hm = static_cast<u16>(mant >> 13);
    return static_cast<u16>((sign << 15) | (he << 10) | hm);
}
static inline uint32_t pack_half2x16(f32 u, f32 v)
{
    return static_cast<u16>(float_to_half(u)) | (static_cast<u16>(float_to_half(v)) << 16);
}

// ===== u16×2 uint packing =====
static inline void push_u16_pair(std::vector<u32>& out, u16 a, u16 b) 
{
    out.push_back((static_cast<u32>(b) << 16) | static_cast<u32>(a));
}
