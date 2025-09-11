#pragma once

#include "pch.h"

struct rhiRenderTargetView;
struct renderTargetViewDesc;
struct viewKey 
{
    VkImage image;
    VkFormat format;
    VkImageAspectFlags aspect;
    u32 mip;
    u32 base_layer;
    u32 layer_count;

    bool operator==(const viewKey& o) const noexcept;
};

struct viewKeyHash 
{
    size_t operator()(const viewKey& k) const noexcept;
};

class vkImageViewCache 
{
public:
    explicit vkImageViewCache(VkDevice device) : device(device) {}
    ~vkImageViewCache();

    void clear();
    VkImageView get_or_create(const rhiRenderTargetView* view);
    VkImageView get_or_create(const viewKey& key, const VkComponentMapping* swizzle = nullptr);
    VkImageView get(const renderTargetViewDesc* desc);

    void delete_imageview(VkImage image);

private:
    VkDevice device;
    std::unordered_map<viewKey, VkImageView, viewKeyHash> cache;
    std::unordered_map<VkImage, std::vector<VkImageView>> reverse;
};