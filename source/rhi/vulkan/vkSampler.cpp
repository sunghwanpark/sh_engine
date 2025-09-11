#include "vkSampler.h"
#include "vkCommon.h"
#include "vkDeviceContext.h"

vkSampler::vkSampler(vkDeviceContext* context, const rhiSamplerDesc& desc)
	: device(context->device)
{
	VkSamplerCreateInfo create_info
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = vk_filter(desc.mag_filter),
		.minFilter = vk_filter(desc.min_filter),
		.mipmapMode = vk_mipmap(desc.mipmap_mode),
		.addressModeU = vk_address_mode(desc.address_u),
		.addressModeV = vk_address_mode(desc.address_v),
		.addressModeW = vk_address_mode(desc.address_w),
		.mipLodBias = desc.mip_lodbias,
		.anisotropyEnable = desc.enable_anisotropy,
		.maxAnisotropy = desc.max_anisotropy,
		.compareEnable = desc.enable_compare,
		.compareOp = vk_compare_op(desc.compare_op),
		.minLod = desc.min_lod,
		.maxLod = desc.max_lod,
		.borderColor = vk_border_color(desc.border),
		.unnormalizedCoordinates = desc.unnormalized_coords
	};
	VK_CHECK_ERROR(vkCreateSampler(device, &create_info, nullptr, &sampler));
}

vkSampler::~vkSampler()
{
	vkDestroySampler(device, sampler, nullptr);
}