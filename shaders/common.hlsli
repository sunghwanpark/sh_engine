#pragma pack_matrix(column_major)

struct instanceData 
{ 
    float4x4 model; 
    float4x4 normal_mat;
};

float3 srgb_to_linear(float3 c) { return pow(c, 2.2.xxx); }
