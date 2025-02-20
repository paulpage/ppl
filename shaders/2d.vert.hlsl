struct VertexData {
    float4 dst_rect;
    float4 src_rect;
    float4 border_color;
    float4 corner_radii;
    float4 colors[4];
    float edge_softness;
    float border_thickness;
    float use_texture;
};

struct Output {
    float4 rect : RECT;
    float4 color : COLOR;
    float4 border_color : BCOLOR;
    float4 corner_radii : RADII;
    float4 position : SV_Position;
    float2 tex_coord : TEXCOORD0;
    float border_thickness : BTHICKNESS;
    float use_texture : USETEX;
};

StructuredBuffer<VertexData> data : register(t0, space0);

cbuffer UniformBlock : register(b0, space1) {
    float2 screen_size : packoffset(c0);
};

static const uint tri_idx[6] = {0, 1, 2, 2, 3, 0};

Output main(uint id : SV_VertexID) {

    VertexData d = data[id / 6];
    uint p = id % 6;

    float2 vert_pos[4] = {
        float2(d.dst_rect.x, d.dst_rect.y),
        float2(d.dst_rect.x, d.dst_rect.y + d.dst_rect.w),
        float2(d.dst_rect.x + d.dst_rect.z, d.dst_rect.y + d.dst_rect.w),
        float2(d.dst_rect.x + d.dst_rect.z, d.dst_rect.y),
    };

    float2 tex_coords[4] = {
        float2(d.src_rect.x, d.src_rect.y),
        float2(d.src_rect.x, d.src_rect.y + d.src_rect.w),
        float2(d.src_rect.x + d.src_rect.z, d.src_rect.y + d.src_rect.w),
        float2(d.src_rect.x + d.src_rect.z, d.src_rect.y),
    };

    Output output;
    output.tex_coord = tex_coords[tri_idx[p]];
    output.color = d.colors[tri_idx[p]];
    output.position = float4((vert_pos[tri_idx[p]] / (screen_size / 2) - 1) * float2(1, -1), 0, 1);
    output.rect = d.dst_rect;
    output.corner_radii = d.corner_radii;
    output.border_color = d.border_color;
    output.border_thickness = d.border_thickness;
    output.use_texture = d.use_texture;
    return output;
}
