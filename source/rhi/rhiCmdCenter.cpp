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

#if ENABLE_AFTERMATH
void rhiCmdCenter::on_gpu_crash_dump(const void* data, const u32 size, void* user)
{
    if (auto* self = static_cast<rhiCmdCenter*>(user)) 
    {
        self->handle_gpu_crashdump(data, size);
    }
}

void rhiCmdCenter::on_shader_debug_info(const void* data, const u32 size, void* user)
{
    if (auto* self = static_cast<rhiCmdCenter*>(user))
    {
        self->handle_shader_debuginfo(data, size);
    }
}

void rhiCmdCenter::on_description(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription add, void* user)
{
}

void rhiCmdCenter::on_resolve_marker(const void* marker, const u32 marker_size, void* user, void** out_ptr, u32* out_size)
{
}

void rhiCmdCenter::handle_gpu_crashdump(const void* dump, u32 size)
{
    const std::filesystem::path out_dir = std::filesystem::u8path("crash_dumps") / now_stamp();
    const std::filesystem::path dump_path = out_dir / "crash.nv-gpudmp";
    save_binary(dump_path, dump, size);

    GFSDK_Aftermath_GpuCrashDump_Decoder decoder = nullptr;
    if (GFSDK_Aftermath_GpuCrashDump_CreateDecoder(GFSDK_Aftermath_Version_API, dump, size, &decoder) == GFSDK_Aftermath_Result_Success)
    {
        u32 json_size = 0;
        const auto dFlags = GFSDK_Aftermath_GpuCrashDumpDecoderFlags_SHADER_INFO |
            GFSDK_Aftermath_GpuCrashDumpDecoderFlags_WARP_STATE_INFO |
            GFSDK_Aftermath_GpuCrashDumpDecoderFlags_SHADER_MAPPING_INFO;
        const auto fFlags = GFSDK_Aftermath_GpuCrashDumpFormatterFlags_CONDENSED_OUTPUT;

        if (GFSDK_Aftermath_GpuCrashDump_GenerateJSON(decoder, dFlags, fFlags, nullptr, nullptr, nullptr, this, &json_size) == GFSDK_Aftermath_Result_Success 
            && json_size > 0)
        {
            std::string json(json_size, '\0');
            GFSDK_Aftermath_GpuCrashDump_GetJSON(decoder, json_size, json.data());
            save_text(out_dir / "crash.json", json);
        }
        GFSDK_Aftermath_GpuCrashDump_DestroyDecoder(decoder);
    }
}

void rhiCmdCenter::handle_shader_debuginfo(const void* info, u32 size)
{
    const std::filesystem::path out_dir = std::filesystem::path("crash_dumps") / now_stamp();
    save_binary(out_dir / "shader_debug.nvdbg", info, size);
}
#endif