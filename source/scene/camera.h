#pragma once

#include "pch.h"

class camera
{
public:
	camera(GLFWwindow* window);
	static void mouse_dispatch(GLFWwindow* window, f64 pos_x, f64 pos_y);
	static void mouse_button_dispatch(GLFWwindow* window, i32 button, i32 action, i32 mods);

public:
	void process_input(GLFWwindow* window, f32 delta);
	mat4 view() const;
	mat4 proj(const vec2 size) const;
	vec3 get_position() { return position; }
	vec3 get_front() { return front; }
	f32 get_near() { return znear; }
	f32 get_far() { return zfar; }

private:
	void update_front();
	void mouse_move(f64 pos_x, f64 pos_y);
	void mouse_action(i32 button, i32 action, i32 mods);

private:
	GLFWwindow* window;

	vec3 position = { 10.f, 6.f, 10.f };
	f32 yaw = -135.f;
	f32 pitch = -10.f;
	f32 move_speed = 5.0f;
	f32 mouse_sens = 0.1f;
	vec3 front = { 0.f, 0.f, -1.f };
	vec3 up = { 0.f, 1.f, 0.f };

	bool is_clicked = false;
	bool first = true;

	const f32 sensitivity = 0.1f;
	const f32 znear = 0.1f;
	const f32 zfar = 300.0f;
	f64vec2 last_mouse_pos;
};
