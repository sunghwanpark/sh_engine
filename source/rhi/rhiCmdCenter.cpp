#include "rhiCmdCenter.h"
#include "vulkan/vkCmdCenter.h"

std::unique_ptr<rhiCmdCenter> rhiCmdCenter::create_cmd_center(rhi_type backend)
{
    switch (backend) 
    {
    case rhi_type::vulkan: return std::make_unique<vkCmdCenter>();
    //case rhi_type::dx12:  return std::make_unique<dxCmdCenter>();
    }
    return {};
}