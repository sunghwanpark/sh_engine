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
#if ENABLE_AFTERMATH
		GFSDK_Aftermath_EnableGpuCrashDumps(
			GFSDK_Aftermath_Version_API,
			GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
			GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
			&rhiCmdCenter::on_gpu_crash_dump,
			&rhiCmdCenter::on_shader_debug_info,
			&rhiCmdCenter::on_description,
			&rhiCmdCenter::on_resolve_marker, this);
#endif
		return true; 
	}

	std::weak_ptr<rhiDeviceContext> get_device_context() { return device_context; }
	std::weak_ptr<rhiFrameContext> get_frame_context() { return frame_context; }

private:
#if ENABLE_AFTERMATH
	static void on_gpu_crash_dump(const void* data, const u32 size, void* user);
	static void on_shader_debug_info(const void* data, const u32 size, void* user);
	static void on_description(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add, void* user);
	static void on_resolve_marker(const void* marker, const u32 marker_size, void* user, void** out_ptr, u32* out_size);

	void handle_gpu_crashdump(const void* dump, u32 size);
	void handle_shader_debuginfo(const void* info, u32 size);
#endif
protected:
	std::shared_ptr<rhiDeviceContext> device_context;
	std::shared_ptr<rhiFrameContext> frame_context;
};
