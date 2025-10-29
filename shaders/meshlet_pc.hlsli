struct materialPC
{
    uint base_color_index;
    uint norm_color_index;
    uint mr_color_index;
    uint base_sampler_index;
    uint norm_sampler_index;
    uint mr_sampler_index;

    float alpha_cutoff;
    float metalic_factor;
    float roughness_factor;
    float __pad[3];
}; // 48b

struct drawparamPC
{
    uint dp_index;
    uint __pad[3];
}; // 16b

struct pushConstant
{
    drawparamPC dp;
    [[vk::offset(16)]]
    materialPC materials;
};

[[vk::push_constant]]
ConstantBuffer<pushConstant> pc;