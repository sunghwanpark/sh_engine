struct meshletDrawParams
{
    uint first_meshlet;
    uint meshlet_count;
    uint first_instance;
    uint instance_count;
};

struct meshletHeader
{
    uint vertex_count;
    uint prim_count;
    uint vertex_offset;
    uint prim_byte_offset;
};

// 10:10:10:2 SNORM ¡æ float3 ([-1,1])
float3 unpack1010102_SNORM(uint p)
{
    uint rx =  p        & 0x3FFu;
    uint ry = (p >> 10) & 0x3FFu;
    uint rz = (p >> 20) & 0x3FFu;

    int sx = (rx & 0x200u) ? (int)(rx | ~0x3FFu) : (int)rx;
    int sy = (ry & 0x200u) ? (int)(ry | ~0x3FFu) : (int)ry;
    int sz = (rz & 0x200u) ? (int)(rz | ~0x3FFu) : (int)rz;

    float3 v = float3( (float)sx/511.0f, (float)sy/511.0f, (float)sz/511.0f );
    return normalize(v);
}

int unpack_handed(uint p)
{
    uint a = (p >> 30) & 0x3u;
    return (a & 1u) ? +1 : -1;
}

float half_to_float(uint hbits)
{
    uint s = (hbits >> 15) & 1u;
    uint e = (hbits >> 10) & 0x1Fu;
    uint f = (hbits) & 0x3FFu;
    if (e == 0)
    {
        if (f == 0) 
            return asfloat(s << 31);
        // subnormal
        float mant = (float)f / 1024.0f;
        float val  = ldexp(mant, -14);
        return s ? -val : val;
    }
    if (e == 31)
    {
        uint bits = (s << 31) | 0x7F800000u | (f ? 1u : 0u);
        return asfloat(bits);
    }
    int   E    = (int)e - 15 + 127;
    uint  bits = (s << 31) | (uint(E) << 23) | (f << 13);
    return asfloat(bits);
};

float2 unpack_half2(uint packed)
{
    uint hu =  packed & 0xFFFFu;
    uint hv = (packed >> 16) & 0xFFFFu;
    return float2(half_to_float(hu), half_to_float(hv));
}

uint load_u16_pair(StructuredBuffer<uint> buf, uint pairIndex, bool hi)
{
    uint packed = buf[pairIndex];
    return hi ? (packed >> 16) & 0xFFFFu : (packed & 0xFFFFu);
}

uint load_tri_index(ByteAddressBuffer tri_bytes, uint byte_offset)
{
    uint dword_offset = byte_offset & ~3u;
    uint shift= (byte_offset & 3u) * 8u;
    uint val32 = tri_bytes.Load(dword_offset);
    return (val32 >> shift) & 0xFFu;
}
