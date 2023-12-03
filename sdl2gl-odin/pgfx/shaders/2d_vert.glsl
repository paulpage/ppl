#version 330 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 tex_coords;

out vec2 v_tex_coords;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    v_tex_coords = tex_coords;
}

// #version 330 core

// layout(location=0) in vec3 a_position;
// layout(location=1) in vec4 a_color;

// out vec4 v_color;

// uniform mat4 u_transform;

// void main() {
// 	gl_Position = u_transform * vec4(a_position, 1.0);
// 	v_color = a_color;
// }
