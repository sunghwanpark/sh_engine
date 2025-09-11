#include "camera.h"

camera::camera(GLFWwindow* window)
    : window(window)
{
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, mouse_dispatch);
    glfwSetMouseButtonCallback(window, mouse_button_dispatch);
}

void camera::mouse_dispatch(GLFWwindow* window, f64 pos_x, f64 pos_y)
{
    auto* self = static_cast<camera*>(glfwGetWindowUserPointer(window));
    if (self) 
    {
        self->mouse_move(pos_x, pos_y);
    }
}

void camera::mouse_button_dispatch(GLFWwindow* window, i32 button, i32 action, i32 mods)
{
    auto* self = static_cast<camera*>(glfwGetWindowUserPointer(window));
    if (self)
    {
        self->mouse_action(button, action, mods);
    }
}

void camera::process_input(GLFWwindow* window, f32 delta)
{
    const f32 v = move_speed * delta;
    const vec3 right = glm::normalize(glm::cross(up, front));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += front * v;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) 
        position -= front * v;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += right * v;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= right * v;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        position += up * v;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) 
        position -= up * v;
}

void camera::mouse_move(f64 pos_x, f64 pos_y)
{
    if (!is_clicked)
        return;

    if (first) 
    {
        last_mouse_pos.x = pos_x;
        last_mouse_pos.y = pos_y;
        first = false;
    }

    f32 offset_x = static_cast<f32>(pos_x - last_mouse_pos.x);
    f32 offset_y = static_cast<f32>(last_mouse_pos.y - pos_y);
    last_mouse_pos.x = pos_x;
    last_mouse_pos.y = pos_y;

    offset_x *= sensitivity;
    offset_y *= sensitivity;

    yaw -= offset_x;
    pitch += offset_y;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    update_front();
}

void camera::mouse_action(i32 button, i32 action, i32 mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT) 
    {
        if (action == GLFW_PRESS) 
        {
            is_clicked = true;
            first = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwGetCursorPos(window, &last_mouse_pos.x, &last_mouse_pos.y);
        }
        else if (action == GLFW_RELEASE) 
        {
            is_clicked = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void camera::update_front()
{
    f32 cy = glm::cos(glm::radians(yaw));
    f32 sy = glm::sin(glm::radians(yaw));
    f32 cp = glm::cos(glm::radians(pitch));
    f32 sp = glm::sin(glm::radians(pitch));
    front = glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
}

mat4 camera::view() const
{
    return glm::lookAtLH(position, position + front, up);
}

mat4 camera::proj(const vec2 size) const
{
    constexpr f32 fovy = glm::radians(60.0f);
    const f32 aspect = float(size.x) / float(size.y);
    glm::mat4 P = glm::perspectiveLH(fovy, aspect, znear, zfar);
    P[1][1] *= -1.0f;
    return P;
}