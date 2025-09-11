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

int main() 
{
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
