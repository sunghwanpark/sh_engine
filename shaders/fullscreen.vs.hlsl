// fullscreen.vs.hlsl
struct vsOut 
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

vsOut main(uint vid : SV_VertexID)
{
    float2 pos = float2((vid == 2) ? 3.0 : -1.0, (vid == 1) ? 3.0 : -1.0);
    float2 uv = float2((vid == 2) ? 2.0 : 0.0, (vid == 1) ? 2.0 : 0.0);

    vsOut o;
    o.pos = float4(pos, 0, 1);
    o.uv = uv;
    return o;
}
