// gbuffer.ps.hlsl

struct psIn
{
    float4 pos : SV_Position;
    float3 normal : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

struct psOut
{
    float4 a : SV_Target0;
};

psOut main(psIn i)
{
    psOut o;
;    o.a = float4(float3(1.f, 0.f, 0.f), 1);
    return o;
}
