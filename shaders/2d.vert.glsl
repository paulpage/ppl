#version 450

layout (location = 0) in vec4 in_dst_rect;
layout (location = 1) in vec4 in_src_rect;
layout (location = 2) in vec4 in_color0;
layout (location = 3) in vec4 in_color1;
layout (location = 4) in vec4 in_color2;
layout (location = 5) in vec4 in_color3;
layout (location = 6) in float in_corner_radius;
layout (location = 7) in float in_edge_softness;
layout (location = 8) in float in_border_thickness;

layout (location = 0) out vec4 out_dst_rect;
layout (location = 1) out vec4 out_src_rect;
layout (location = 2) out vec4 out_color0;
layout (location = 3) out vec4 out_color1;
layout (location = 4) out vec4 out_color2;
layout (location = 5) out vec4 out_color3;
layout (location = 6) out float out_corner_radius;
layout (location = 7) out float out_edge_softness;
layout (location = 8) out float out_border_thickness;

layout(set = 1, binding = 0) uniform ScreenSize {
    vec2 u_screen_size;
};

void main()
{

    vec2 pos = vec2(0.0, 0.0);
    switch (gl_VertexIndex) {
        case 0:
            pos = vec2(-0.5, -0.5);
            out_color0 = in_color0;
            break;
        case 1:
            pos = vec2(0.5, -0.5);
            out_color0 = in_color1;
            break;
        case 2:
            pos = vec2(-0.5, 0.5);
            out_color0 = in_color2;
            break;
        case 3:
            pos = vec2(0.5, 0.5);
            out_color0 = in_color3;
            break;
    }
    gl_Position = vec4(pos, 0, 1);


    out_dst_rect = in_dst_rect;
    out_src_rect = in_src_rect;
    //out_color0 = in_color0;
    out_color1 = in_color1;
    out_color2 = in_color2;
    out_color3 = in_color3;
    out_corner_radius = in_corner_radius;
    out_edge_softness = in_edge_softness;
    out_border_thickness = in_border_thickness;
    // outPos = (position / uScreenSize) * 2.0 - 1.0;
    // outPos.y = -outPos.y;
    // outColor = color / 255.0;
    // outRectMin = (rectMin / uScreenSize) * 2.0 - 1.0;
    // outRectMax = (rectMax / uScreenSize) * 2.0 - 1.0;
	// gl_Position = vec4(outPos, 0, 1);
}
