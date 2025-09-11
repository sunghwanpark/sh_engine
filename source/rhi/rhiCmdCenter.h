#pragma once
#include "pch.h"

class rhiFrameContext;
class rhiDeviceContext;
class rhiCmdCenter
{
public:
	static std::unique_ptr<rhiCmdCenter> create_cmd_center(rhi_type backend);

	void clear()
	{
		device_context.reset();
		frame_context.reset();
	}

	virtual bool initialize(std::string_view application_name, GLFWwindow* window, const u32 width, const u32 height) 
	{
		return true; 
	}

	std::weak_ptr<rhiDeviceContext> get_device_context() { return device_context; }
	std::weak_ptr<rhiFrameContext> get_frame_context() { return frame_context; }

protected:
	std::shared_ptr<rhiDeviceContext> device_context;
	std::shared_ptr<rhiFrameContext> frame_context;
};
