#define PI 3.1415926535

float radical_inverse_vdc(uint bits)
{
    // Van der Corput base 2
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1) | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2) | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4) | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8) | ((bits & 0xFF00FF00u) >> 8);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 hammersley(uint i, uint n)
{
    return float2((float)i / (float)n, radical_inverse_vdc(i));
}

void build_tangent_frame(float3 n, out float3 t, out float3 b)
{
    float3 up = (abs(n.z) < 0.999f) ? float3(0, 0, 1) : float3(0, 1, 0);
    t = normalize(cross(up, n));
    b = cross(n, t);
}

float3 face_dir(uint face, float2 uv) // uv: [-1,1]
{
    float x = uv.x;
    float y = uv.y;
    [branch]
    if (face == 0) return normalize(float3(1, -y, -x)); // +X
    else if (face == 1) return normalize(float3(-1, -y, x)); // -X
    else if (face == 2) return normalize(float3(x, 1, y)); // +Y
    else if (face == 3) return normalize(float3(x, -1, -y)); // -Y
    else if (face == 4) return normalize(float3(x, -y, 1)); // +Z
    else return normalize(float3(-x, -y, -1));    // -Z
}

// GGX importance sample (half-vector H) with alpha=a
float3 importance_sample_GGX_H(float2 xi, float a, float3 n)
{
    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));

    float3 ht = float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);

    float3 t;
    float3 b;
    build_tangent_frame(n, t, b);
    float3 h = normalize(t * ht.x + b * ht.y + n * ht.z);
    return h;
}