#pragma once

#include "pch.h"

class rhiCommandList;
struct rhiPipelineLayout;
struct rhiDescriptorSetLayout;
class rhiBindlessTable 
{
public:
    virtual void bind_once(rhiCommandList* cmd, rhiPipelineLayout layout, u32 setindex_or_rootslot) = 0;
    virtual rhiDescriptorSetLayout get_set_layout() = 0;
};
