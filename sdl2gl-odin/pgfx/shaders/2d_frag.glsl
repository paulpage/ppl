#version 330 core

uniform sampler2D tex;

in vec2 v_pos;
in vec2 v_tex_coords;
in vec4 v_color;

out vec4 f_color;

void main() {
    f_color = texture(tex, v_tex_coords) * v_color;
}

// #version 330 core

// in vec4 v_color;

// out vec4 o_color;

// void main() {
// 	o_color = v_color;
// }
