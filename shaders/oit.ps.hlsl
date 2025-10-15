// oit.ps.hlsl

struct psIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D accum_tex : register(t0, space0);
Texture2D reveal_tex : register(t1, space0);
SamplerState linear_sampler : register(s2, space0);

float4 main(psIn i) : SV_Target0
{
    float4 accum = accum_tex.Sample(linear_sampler,  i.uv);
    float trans = saturate(reveal_tex.Sample(linear_sampler, i.uv).r);

    float3 avg_color = (accum.a > 1e-6) ? (accum.rgb / accum.a) : float3(0,0,0);

    // Color = avg_color, Alpha = 1 - trans  -> Dst * (1 - SrcA) = Dst * trans
    return float4(avg_color, 1.0 - trans);
}