struct instanceData 
{ 
    float4x4 model; 
    float3x3 normal_mat;
};

float3 srgb_to_linear(float3 c) { return pow(c, 2.2.xxx); }
