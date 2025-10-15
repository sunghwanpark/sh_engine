#include "pch.h"
#include "mesh/glTFMesh.h"
#include "rhi/rhiCmdCenter.h"
#include "scene/sponzaScene.h"
#include "renderer/renderer.h"

class engine
{
public:
    engine(const rhi_type t) : type(t) {}
    ~engine() = default;

    void init(std::string_view application_name, GLFWwindow* window)
    {
        constexpr u32 width = 1920;
        constexpr u32 height = 1080;

        cmd_center = rhiCmdCenter::create_cmd_center(rhi_type::vulkan);
        cmd_center->initialize(application_name, window, width, height);
        
        s = std::make_unique<sponzaScene>();
        s->start_scene(window);

        auto device_context_ptr = cmd_center->get_device_context().lock();
        auto frame_context_ptr = cmd_center->get_frame_context().lock();
        r = std::make_unique<renderer>();
        r->initialize(s.get(), device_context_ptr.get(), frame_context_ptr.get());
    }

    void update(GLFWwindow* window, float delta)
    {
        s->update(window, delta);
    }

    void render()
    {
        r->pre_render(s.get());
        r->render(s.get());
        r->post_render();
    }

    void exit()
    {
        r.reset();
        s.reset();
        cmd_center.reset();
    }

private:
    rhi_type type;
    std::unique_ptr<rhiCmdCenter> cmd_center;
    std::unique_ptr<scene> s;
    std::unique_ptr<renderer> r;
};

static void append_text_winapi(const wchar_t* path, const char* data, DWORD len) 
{
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) 
        return;
    DWORD w = 0; WriteFile(h, data, len, &w, nullptr);
    CloseHandle(h);
}

static void SymbolizeAndAppend_SEH(const wchar_t* dump_path_w, CONTEXT* pctx)
{
    __try 
    {
        char line[1024];

        CONTEXT ctx = *pctx;
        STACKFRAME64 sf{};
#if defined(_M_X64)
        DWORD mach = IMAGE_FILE_MACHINE_AMD64;
        sf.AddrPC.Offset = ctx.Rip;
        sf.AddrFrame.Offset = ctx.Rbp;
        sf.AddrStack.Offset = ctx.Rsp;
#elif defined(_M_IX86)
        DWORD mach = IMAGE_FILE_MACHINE_I386;
        sf.AddrPC.Offset = ctx.Eip;
        sf.AddrFrame.Offset = ctx.Ebp;
        sf.AddrStack.Offset = ctx.Esp;
#endif
        sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;

        const char hdr[] = "-- backtrace (symbolized) --\r\n";
        append_text_winapi(dump_path_w, hdr, (DWORD)sizeof(hdr) - 1);

        HANDLE p = GetCurrentProcess();
        HANDLE t = GetCurrentThread();

        for (unsigned i = 0; i < 64; ++i) 
        {
            if (!StackWalk64(mach, p, t, &sf, &ctx, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) 
                break;
            if (sf.AddrPC.Offset == 0) 
                break;

            char si_mem[sizeof(SYMBOL_INFO) + 256];
            auto* si = reinterpret_cast<SYMBOL_INFO*>(si_mem);
            si->SizeOfStruct = sizeof(SYMBOL_INFO);
            si->MaxNameLen = 255;

            DWORD64 disp = 0;
            if (SymFromAddr(p, (DWORD64)sf.AddrPC.Offset, &disp, si)) 
            {
                IMAGEHLP_LINE64 l{}; l.SizeOfStruct = sizeof(l);
                DWORD ld = 0;
                if (SymGetLineFromAddr64(p, (DWORD64)sf.AddrPC.Offset, &ld, &l)) 
                {
                    int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "%p %s + 0x%X  (%s:%lu)\r\n", (void*)sf.AddrPC.Offset, si->Name, (unsigned)disp, l.FileName ? l.FileName : "?", l.LineNumber);
                    if (n > 0) 
                        append_text_winapi(dump_path_w, line, (DWORD)n);
                }
                else 
                {
                    int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "%p %s + 0x%X\r\n", (void*)sf.AddrPC.Offset, si->Name, (unsigned)disp);
                    if (n > 0) 
                        append_text_winapi(dump_path_w, line, (DWORD)n);
                }
            }
            else 
            {
                int n = _snprintf_s(line, sizeof(line), _TRUNCATE, "%p\r\n", (void*)sf.AddrPC.Offset);
                if (n > 0) 
                    append_text_winapi(dump_path_w, line, (DWORD)n);
            }
        }
        const char ftr[] = "=======================\r\n";
        append_text_winapi(dump_path_w, ftr, (DWORD)sizeof(ftr) - 1);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) 
    {
        const char msg[] = "-- symbolization failed --\r\n";
        append_text_winapi(dump_path_w, msg, (DWORD)sizeof(msg) - 1);
    }
}
LONG WINAPI crash_dump(EXCEPTION_POINTERS* ex)
{
    const auto out_dir = std::filesystem::path("crash_dumps") / now_stamp();
    std::error_code ec; std::filesystem::create_directories(out_dir, ec);
    const auto dump_path = out_dir / "crash_log.txt";

    std::string buf; buf.reserve(4096);
    std::format_to(std::back_inserter(buf), "=== CRASH (Windows) ===\n");
    if (ex && ex->ExceptionRecord) 
    {
        std::format_to(std::back_inserter(buf), "code = 0x{:08X} addr = {:p}\n", (unsigned long)ex->ExceptionRecord->ExceptionCode, ex->ExceptionRecord->ExceptionAddress);
    }
    std::format_to(std::back_inserter(buf), "-- backtrace (raw addresses) --\n");
    void* frames[64]{};
    USHORT n = RtlCaptureStackBackTrace(0, 64, frames, nullptr);
    for (USHORT i = 0; i < n; ++i)
        std::format_to(std::back_inserter(buf), "{:p}\n", frames[i]);
    std::format_to(std::back_inserter(buf), "-----------------------\n");

    {
        std::ofstream f(dump_path, std::ios::binary);
        if (f) 
            f.write(buf.data(), (std::streamsize)buf.size());
    }

    const std::wstring dump_path_w = dump_path.wstring();
    SymbolizeAndAppend_SEH(dump_path_w.c_str(), ex->ContextRecord);

    return EXCEPTION_EXECUTE_HANDLER;
}

int main() 
{
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    SetUnhandledExceptionFilter(crash_dump);

    engine* e = new engine(rhi_type::vulkan);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto window = glfwCreateWindow(1920, 1080, "App", nullptr, nullptr);
    if (!window) 
    {
        glfwTerminate();
        std::cout << "Creat window error" << std::endl;
        return -1;
    }

    f32 last_frame = 0.f;
    e->init("App", window);
    while (!glfwWindowShouldClose(window)) 
    {
        const f32 current_frame = static_cast<f32>(glfwGetTime());
        const f32 delta_time = current_frame - last_frame;
        last_frame = current_frame;

        e->update(window, delta_time);
        e->render();
        glfwPollEvents();
    }

    e->exit();
    glfwDestroyWindow(window);
    glfwTerminate();

    delete e;
    return 0;
}
