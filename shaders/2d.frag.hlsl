Texture2D<float4> texture : register(t0, space2);
SamplerState sam : register(s0, space2);

struct Input {
    float4 rect : RECT;
    float4 color : COLOR;
    float4 border_color : BCOLOR;
    float4 corner_radii : RADII;
    float4 position : SV_Position; // clip space!
    float2 tex_coord : TEXCOORD0;
    float border_thickness : BTHICKNESS;
    float use_texture : USETEX;
};

cbuffer UniformBlock : register(b0, space3) {
    float2 screen_size : packoffset(c0);
};

float sdf_rounded_box(float2 p, float2 b, float4 r) {
    r.xy = (p.x>0.0)?r.xy : r.zw;
    r.x  = (p.y>0.0)?r.x  : r.y;
    float2 q = abs(p)-b+r.x;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0)) - r.x;
}

float4 main(Input input) : SV_Target0 {
    if (input.use_texture > 0) {
        return input.color * texture.Sample(sam, input.tex_coord);
    }

    float2 half_size = 2 * input.rect.zw / screen_size.y / 2;
    float2 p = (2 * input.position.xy - screen_size.xy) / screen_size.y - (2 * (input.rect.xy + input.rect.zw / 2) - screen_size.xy) / screen_size.y;
    float d = sdf_rounded_box(p, half_size, input.corner_radii / (screen_size.y / 2));

    float d2 = 0;
    float2 half_size2 = 2 * (input.rect.zw - (input.border_thickness * 2 + 2)) / screen_size.y / 2;
    if (input.border_thickness > 0) {
        d2 = sdf_rounded_box(p, half_size2, (input.corner_radii - (input.border_thickness + 2)) / (screen_size.y / 2));
    }

    float4 final_color = lerp(
        float4(input.border_color.xyz, (1-smoothstep(0, 0.003, d)) * input.border_color.a),
        input.color,
        1-smoothstep(0, 0.005, d2)
    );

     return final_color;
}
