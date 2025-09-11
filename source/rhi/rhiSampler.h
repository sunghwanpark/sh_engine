#pragma once

#include "pch.h"
#include "rhi/rhiDefs.h"
#include "util/hash.h"

struct rhiSamplerDesc 
{
    rhiFilter mag_filter = rhiFilter::linear;
    rhiFilter min_filter = rhiFilter::linear;
    rhiMipmapMode mipmap_mode = rhiMipmapMode::linear;

    rhiAddressMode address_u = rhiAddressMode::clamp_to_edge;
    rhiAddressMode address_v = rhiAddressMode::clamp_to_edge;
    rhiAddressMode address_w = rhiAddressMode::clamp_to_edge;

    float mip_lodbias = 0.0f;
    float min_lod = 0.0f;
    float max_lod = 1000.0f;

    bool enable_anisotropy = false;
    float max_anisotropy = 1.0f;

    bool enable_compare = false;
    rhiCompareOp compare_op = rhiCompareOp::less_equal;

    rhiBorderColor border = rhiBorderColor::opaque_black;
    bool unnormalized_coords = false;
};

struct rhiSamplerKey
{
    rhiFilter mag_filter;
    rhiFilter min_filter;
    rhiMipmapMode mipmap_mode;
    rhiAddressMode address_u;
    rhiAddressMode address_v;
    rhiAddressMode address_w;

    rhiSamplerKey(const rhiSamplerDesc& desc)
        : mag_filter(desc.mag_filter), min_filter(desc.min_filter), mipmap_mode(desc.mipmap_mode), address_u(desc.address_u), address_v(desc.address_v), address_w(desc.address_w)
    { }

    bool operator==(const rhiSamplerKey& o) const noexcept
    {
        return mag_filter == o.mag_filter 
            && min_filter == o.min_filter
            && mipmap_mode == o.mipmap_mode
            && address_u == o.address_u
            && address_v == o.address_v
            && address_w == o.address_w;
    }
};

struct rhiSamplerKeyHash
{
    u64 operator()(const rhiSamplerKey& k) const noexcept
    {
        u64 h = 1469598103934665603ull;
        h = hash_combine(h, k.mag_filter);
        h = hash_combine(h, k.min_filter);
        h = hash_combine(h, k.mipmap_mode);
        h = hash_combine(h, k.address_u);
        h = hash_combine(h, k.address_v);
        h = hash_combine(h, k.address_w);
        return h;
    }
};

class rhiSampler
{
public:
    virtual ~rhiSampler() = default;
    rhiSamplerDesc desc;
};