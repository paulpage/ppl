#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PJP_IMPLEMENTATION
#include "pjp.h"

#include "types.h"

#define ASSERT_CALL(call) \
    do { \
        if (!(call)) { \
            SDL_Log("Error in %s: %s", #call, SDL_GetError()); \
            SDL_Quit(); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_CREATED(obj) do { if ((obj) == NULL) { SDL_Log("Error: %s is null", #obj); SDL_Quit(); exit(1); }} while (0)

typedef union Vec4 {
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
} Vec4;

typedef struct Rect {
    float x, y, w, h;
} Rect;

typedef struct Vec2 {
    float x, y;
} Vec2;

typedef struct Texture {
    SDL_GPUTexture *handle;
    int w, h, d;
} Texture;

#define FONT_SIZE 24.0f
#define ATLAS_WIDTH 512
#define ATLAS_HEIGHT 512

typedef struct {
    Texture texture;
    stbtt_packedchar char_data[96];
    float scale;
} Font;

typedef struct VertInput {
    Rect dst_rect;
    Rect src_rect;
    Vec4 border_color;
    Vec4 corner_radii;
    Vec4 colors[4];
    float edge_softness;
    float border_thickness;
    float use_texture;
    float _padding[1]; // std140 alignment
} VertInput;

typedef struct VertStore {
    VertInput *data;
    int size;
    int capacity;
} VertStore;

VertStore make_vert_store() {
    VertInput *data = malloc(1024 * sizeof(VertInput));
    return (VertStore){
        .data = data,
        .size = 0,
        .capacity = 1024,
    };
}

void push_vert(VertStore *store, VertInput input) {
    if (store->size == store->capacity) {
        printf("push capacity %d -> %d\n", store->capacity, store->capacity * 2);
        store->capacity *= 2;
        store->data = realloc(store->data, store->capacity * sizeof(VertInput));
    }

    store->data[store->size] = input;
    store->size++;
}

void vert_clear(VertStore *store) {
    store->size = 0;
}

void free_vert_store(VertStore *store) {
    free(store->data);
    store->data = NULL;
    store->size = 0;
    store->capacity = 0;
}

SDL_GPUShader *load_shader(
    SDL_GPUDevice *gpu,
    char *filename,
    SDL_GPUShaderStage stage,
    int num_samplers,
    int num_storage_textures,
    int num_storage_buffers,
    int num_uniform_buffers
) {
    size_t len;
    unsigned char *data = read_file(filename, &len);
    SDL_GPUShaderCreateInfo info = {
        .code_size = len,
        .code = data,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = stage,
        .num_samplers = num_samplers,
        .num_storage_textures = num_storage_textures,
        .num_storage_buffers = num_storage_buffers,
        .num_uniform_buffers = num_uniform_buffers,
    };

    SDL_GPUShader *shader = SDL_CreateGPUShader(gpu, &info);
    ASSERT_CREATED(shader);
    return shader;
}

Texture load_texture_bytes(SDL_GPUDevice *gpu, u8 *data, int w, int h, int d) {
    SDL_GPUTransferBuffer *texture_transfer_buffer = SDL_CreateGPUTransferBuffer(
        gpu,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = w * h * d,
        }
    );

    u8 *texture_transfer_ptr = SDL_MapGPUTransferBuffer(
        gpu,
        texture_transfer_buffer,
        false
    );
    SDL_memcpy(texture_transfer_ptr, data, w * h * d);
    SDL_UnmapGPUTransferBuffer(gpu, texture_transfer_buffer);

    SDL_GPUTexture *handle;
    if (d == 4) {
        handle = SDL_CreateGPUTexture(
            gpu,
            &(SDL_GPUTextureCreateInfo){
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                .width = w,
                .height = h,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
            }
        );
    } else if (d == 1) {
        handle = SDL_CreateGPUTexture(
            gpu,
            &(SDL_GPUTextureCreateInfo){
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = SDL_GPU_TEXTUREFORMAT_A8_UNORM,
                .width = w,
                .height = h,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
            }
        );
    }

    SDL_GPUCommandBuffer *upload_cmd_buf = SDL_AcquireGPUCommandBuffer(gpu);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd_buf);

    SDL_UploadToGPUTexture(
        copy_pass,
        &(SDL_GPUTextureTransferInfo) {
            .transfer_buffer = texture_transfer_buffer,
            .offset = 0,
        },
        &(SDL_GPUTextureRegion){
            .texture = handle,
            .w = w,
            .h = h,
            .d = 1,
        },
        false
    );

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmd_buf);
    SDL_ReleaseGPUTransferBuffer(gpu, texture_transfer_buffer);

    return (Texture){
        .handle = handle,
        .w = w,
        .h = h,
        .d = d,
    };
}

Texture load_texture(SDL_GPUDevice *gpu, char *filename) {
    int w, h, n;
    u8 *data = stbi_load(filename, &w, &h, &n, 0);
    ASSERT_CREATED(data);

    Texture texture = load_texture_bytes(gpu, data, w, h, n);

    stbi_image_free(data);
    return texture;
}

Font load_font(SDL_GPUDevice *gpu, const char* font_path) {
    Font font = {0};
    font.scale = FONT_SIZE;
    size_t font_size = 0;
    u8 *font_buffer = read_file(font_path, &font_size);
    printf("font file size: %zd\n", font_size);

    u8 *atlas_data = malloc(ATLAS_WIDTH * ATLAS_HEIGHT);

    stbtt_pack_context pack_context = {0};

    stbtt_pack_range pack_range = {0};
    pack_range.font_size = FONT_SIZE;
    pack_range.first_unicode_codepoint_in_range = 32;
    pack_range.num_chars = 96;
    pack_range.chardata_for_range = font.char_data;

    stbtt_PackBegin(&pack_context, atlas_data, ATLAS_WIDTH, ATLAS_HEIGHT, 0, 1, NULL);
    stbtt_PackFontRanges(&pack_context, font_buffer, 0, &pack_range, 1);

    stbtt_PackEnd(&pack_context);

	/*u32* pixels = malloc(ATLAS_WIDTH * ATLAS_HEIGHT * sizeof(u32));*/
	/*const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_RGBA32);*/
	/*for(int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {*/

		/*pixels[i] = SDL_MapRGBA(format, NULL, 0xff, 0xff, 0xff, atlas_data[i]);*/
	/*}*/

    u8 *pixels = malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        pixels[i*4] = 0;
        pixels[i*4 + 1] = 0;
        pixels[i*4 + 2] = 0;
        pixels[i * 4 + 3] = atlas_data[i];
    }
    /*font.texture = load_texture_bytes(gpu, atlas_data, ATLAS_WIDTH, ATLAS_HEIGHT, 1);*/
    font.texture = load_texture_bytes(gpu, pixels, ATLAS_WIDTH, ATLAS_HEIGHT, 4);

	free(pixels);

    /*font.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, ATLAS_WIDTH, ATLAS_HEIGHT);*/
    /*SDL_SetTextureBlendMode(font.texture, SDL_BLENDMODE_BLEND);*/
    /*SDL_UpdateTexture(font.texture, NULL, pixels, ATLAS_WIDTH * sizeof(Uint32));*/
    /**/

    free(atlas_data);
    free(font_buffer);

    return font;
}

void draw_text(VertStore *store, Font *font, const char *text, float x, float y) {
    u8 r, g, b, a;
    y += font->scale;
    while (*text) {
        if (*text >= 32 && *text < 128) {
            stbtt_aligned_quad quad;
            stbtt_GetPackedQuad(font->char_data, ATLAS_WIDTH, ATLAS_HEIGHT, *text - 32, &x, &y, &quad, 1);

            VertInput vert = {
                .dst_rect = (Rect){quad.x0, quad.y0, quad.x1 - quad.x0, quad.y1 - quad.y0},
                /* .src_rect = (Rect){quad.s0 * ATLAS_WIDTH, quad.t0 * ATLAS_HEIGHT, (quad.s1 - quad.s0) * ATLAS_WIDTH, (quad.t1 - quad.t0) * ATLAS_HEIGHT}, */
                /* .src_rect = (Rect){quad.s0, quad.t0, (quad.s1 - quad.s0), (quad.t1 - quad.t0)}, */
                .src_rect = (Rect){quad.s0, quad.t0, (quad.s1 - quad.s0), (quad.t1 - quad.t0)},
                .corner_radii = {0.0f, 0.0f, 0.0f, 0.0f},
                .border_color = {1.0f, 1.0f, 1.0f, 1.0f},
                .colors = {
                    (Vec4){1.0f, 1.0f, 1.0f, 1.0f},
                    (Vec4){1.0f, 1.0f, 1.0f, 1.0f},
                    (Vec4){1.0f, 1.0f, 1.0f, 1.0f},
                    (Vec4){1.0f, 1.0f, 1.0f, 1.0f},
                },
                .edge_softness = 1.0f,
                .border_thickness = 1.0f,
                .use_texture = 1.0f,
            };
            push_vert(store, vert);
        }
        text++;
    }

	/*   float start_x = x;*/
	/**/
	/*   Uint8 r, g, b, a;*/
	/*SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);*/
	/*SDL_SetTextureColorMod(font->texture, r, g, b);*/
	/*SDL_SetTextureAlphaMod(font->texture, a);*/
	/**/
	/*   while (*text) {*/
	/*       if (*text >= 32 && *text < 128) {*/
	/*           stbtt_aligned_quad quad;*/
	/*           stbtt_GetPackedQuad(font->char_data, ATLAS_WIDTH, ATLAS_HEIGHT, *text - 32, &x, &y, &quad, 1);*/
	/**/
	/*           SDL_FRect src_rect = {*/
	/*               quad.s0 * ATLAS_WIDTH,*/
	/*               quad.t0 * ATLAS_HEIGHT,*/
	/*               (quad.s1 - quad.s0) * ATLAS_WIDTH,*/
	/*               (quad.t1 - quad.t0) * ATLAS_HEIGHT*/
	/*           };*/
	/**/
	/*           SDL_FRect dst_rect = {*/
	/*               quad.x0,*/
	/*               quad.y0,*/
	/*               quad.x1 - quad.x0,*/
	/*               quad.y1 - quad.y0*/
	/*           };*/
	/**/
	/*           SDL_RenderTexture(renderer, font->texture, &src_rect, &dst_rect);*/
	/*       }*/
	/*       ++text;*/
	/*   }*/
}

int main(int argc, char *argv[]) {


    size_t file_size = 0, line_count = 0;
    char **buf = read_file_lines("src/main.c", &file_size, &line_count);
    printf("file_size: %zd\nline_count: %zd\n", file_size, line_count);


    float scroll_offset = 0;
    bool mouse_down = false;

    int width = 800;
    int height = 600;

    ASSERT_CALL(SDL_Init(SDL_INIT_VIDEO));

    SDL_Window *window = SDL_CreateWindow("Playground", width, height, 0);
    ASSERT_CREATED(window);

    // GPU Init
    SDL_GPUDevice *gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    ASSERT_CREATED(gpu);
    ASSERT_CALL(SDL_ClaimWindowForGPUDevice(gpu, window));

    // Shaders
    SDL_GPUShader *vertex_shader = load_shader(gpu, "shaders/2d.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 1, 1);
    SDL_GPUShader *fragment_shader = load_shader(gpu, "shaders/2d.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 1);

    // Pipeline
    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(
		gpu,
		&(SDL_GPUGraphicsPipelineCreateInfo){
			.target_info = (SDL_GPUGraphicsPipelineTargetInfo){
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GetGPUSwapchainTextureFormat(gpu, window),
					.blend_state = {
						.enable_blend = true,
						.color_blend_op = SDL_GPU_BLENDOP_ADD,
						.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
						.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
						.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
						.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
						.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
					}
				}}
			},
			.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
			.vertex_shader = vertex_shader,
			.fragment_shader = fragment_shader,
		}
	);
    ASSERT_CREATED(pipeline);

    SDL_ReleaseGPUShader(gpu, vertex_shader);
    SDL_ReleaseGPUShader(gpu, fragment_shader);

    // Textures
    Font font = load_font(gpu, "res/fonts/vera/Vera.ttf");
    Texture texture = load_texture(gpu, "res/bird.png");

    SDL_GPUSampler *sampler = SDL_CreateGPUSampler(
        gpu,
        &(SDL_GPUSamplerCreateInfo){
			.min_filter = SDL_GPU_FILTER_NEAREST,
			.mag_filter = SDL_GPU_FILTER_NEAREST,
			.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
			.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
			.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
        }
    );

    // Buffer data
    VertStore store = make_vert_store();

    /* for (int i = 0; i < 100; i++) { */
    /*     int x = (f32)(i / 10); */
    /*     int y = (f32)(i % 10); */
    /*     VertInput vert = { */
    /*         .dst_rect = (Rect){x * 80.0f, y * 80.0f, 90.0f, 60.0f}, */
    /*         .src_rect = (Rect){0.0f, 0.0f, 1.0f, 1.0f}, */
    /*         .corner_radii = {30.0f, 30.0f, 10.0f, 5.0f}, */
    /*         .border_color = {0.7f, 0.7f, 0.7f, 1.0f}, */
    /*         .colors = { */
    /*             (Vec4){1.0f, 1.0f, 1.0f, 1.0f}, */
    /*             (Vec4){1.0f, 1.0f, 1.0f, 1.0f}, */
    /*             (Vec4){1.0f, 1.0f, 1.0f, 1.0f}, */
    /*             (Vec4){1.0f, 1.0f, 1.0f, 1.0f}, */
    /*             /1* (Vec4){0.4f, 0.4f, 0.4f, 1.0f}, *1/ */
    /*             /1* (Vec4){0.4f, 0.4f, 0.4f, 1.0f}, *1/ */
    /*             /1* (Vec4){0.2f, 0.2f, 0.2f, 1.0f}, *1/ */
    /*             /1* (Vec4){0.2f, 0.2f, 0.2f, 1.0f}, *1/ */
    /*             /1* (Vec4){1.0f, x / 10.0f, y / 10.0f, 1.0f}, *1/ */
    /*             /1* (Vec4){0.0f, 0.0f, 0.0f, 1.0f}, *1/ */
    /*             /1* (Vec4){0.0f, 0.0f, 0.0f, 1.0f}, *1/ */
    /*             /1* (Vec4){0.0f, 0.0f, 0.0f, 1.0f}, *1/ */
    /*         }, */
    /*         .edge_softness = 1.0f, */
    /*         .border_thickness = 1.0f, */
    /*         .use_texture = 1.0f, */
    /*     }; */
    /*     push_vert(&store, vert); */
    /* } */


    SDL_GPUTransferBuffer *vertex_data_transfer_buffer = SDL_CreateGPUTransferBuffer(
        gpu,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = store.capacity * sizeof(VertInput),
        }
    );

    SDL_GPUBuffer *vertex_data_buffer = SDL_CreateGPUBuffer(
        gpu,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            .size = store.capacity * sizeof(VertInput),
        }
    );

    u64 buf_capacity = store.capacity;

    /*VertInput vertices[2] = {*/
    /*    (VertInput){*/
    /*        .dst_rect = (Rect){100.0f, 100.0f, 400.0f, 400.0f},*/
    /*        .src_rect = (Rect){0.0f, 0.0f, 1.0f, 1.0f},*/
    /*        .corner_radii = (Vec4){50.0f, 20.0f, 100.0f, 10.0f},*/
    /*        .border_color = (Vec4){1.0, 1.0, 0.0, 1.0},*/
    /*        .colors = {*/
    /*            (Vec4){1.0f, 1.0f, 1.0f, 1.0f},*/
    /*            (Vec4){1.0f, 1.0f, 1.0f, 1.0f},*/
    /*            (Vec4){1.0f, 1.0f, 1.0f, 1.0f},*/
    /*            (Vec4){1.0f, 1.0f, 1.0f, 1.0f},*/
    /*        },*/
    /*        .edge_softness = 1.0f,*/
    /*        .border_thickness = 20.0f,*/
    /*        .use_texture = 1.0f,*/
    /*    },*/
    /*    (VertInput){*/
    /*        .dst_rect = (Rect){10.0f, 10.0f, 200.0f, 200.0f},*/
    /*        .src_rect = (Rect){0.0f, 0.0f, 1.0f, 1.0f},*/
    /*        .corner_radii = {10.0f, 10.0f, 10.0f, 10.0f},*/
    /*        .border_color = (Vec4){0.2, 0.2, 0.8, 1.0},*/
    /*        .colors = {*/
    /*            (Vec4){0.0f, 0.0f, 0.0f, 1.0f},*/
    /*            (Vec4){0.0f, 0.0f, 0.0f, 1.0f},*/
    /*            (Vec4){0.2f, 0.2f, 0.2f, 1.0f},*/
    /*            (Vec4){0.2f, 0.2f, 0.2f, 1.0f},*/
    /*        },*/
    /*        .edge_softness = 1.0f,*/
    /*        .border_thickness = 10.0f,*/
    /*    },*/
    /*};*/
    /**/

    /*unsigned int buf_size = 2 * sizeof(VertInput);*/

    // Main loop
    bool quit = false;
    SDL_Event event;
    while (!quit) {
        /*SDL_WaitEvent(&event);*/
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    quit = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_Q) {
                        quit = true;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse_down = true;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse_down = false;
                    }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (mouse_down) {
                        scroll_offset += event.motion.yrel;
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    scroll_offset += event.wheel.y * 20.0f;
                    break;
            }
        }


        store.size = 0;

        for (int i = 0; i < line_count; i++) {
            draw_text(&store, &font, buf[i], 0, i * 20 + scroll_offset);
        }
        if (buf_capacity != store.capacity) {
            SDL_ReleaseGPUTransferBuffer(gpu, vertex_data_transfer_buffer);
            vertex_data_transfer_buffer = SDL_CreateGPUTransferBuffer(
                gpu,
                &(SDL_GPUTransferBufferCreateInfo){
                    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                    .size = store.capacity * sizeof(VertInput),
                }
            );
            SDL_ReleaseGPUBuffer(gpu, vertex_data_buffer);
            vertex_data_buffer = SDL_CreateGPUBuffer(
                gpu,
                &(SDL_GPUBufferCreateInfo){
                    .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                    .size = store.capacity * sizeof(VertInput),
                }
            );
            buf_capacity = store.capacity;
        }


        SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(gpu);
        ASSERT_CREATED(cmdbuf);

        SDL_GPUTexture *swapchain_texture = NULL;
        ASSERT_CALL(SDL_AcquireGPUSwapchainTexture(cmdbuf, window, &swapchain_texture, NULL, NULL));

        if (swapchain_texture) {

            VertInput *data_ptr = SDL_MapGPUTransferBuffer(gpu, vertex_data_transfer_buffer, true);

            for (int i = 0; i < store.size; i++) {
                data_ptr[i] = store.data[i];
            }
            /*for (int i = 0; i < rect_count; i++) {*/
            /*    data_ptr[i] = vertices[i];*/
            /*}*/

            // modify data
            // data_ptr[i].dst_rect.x = ...
            /*for (int i = 0; i < 2; i++) {*/
            /*    data_ptr[i] = vertices[i];*/
            /*}*/

            SDL_UnmapGPUTransferBuffer(gpu, vertex_data_transfer_buffer);

            SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmdbuf);
            SDL_UploadToGPUBuffer(
                copy_pass,
                &(SDL_GPUTransferBufferLocation) {
                    .transfer_buffer = vertex_data_transfer_buffer,
                    .offset = 0,
                },
                &(SDL_GPUBufferRegion) {
                    .buffer = vertex_data_buffer,
                    .offset = 0,
                    .size = store.size * sizeof(VertInput),
                },
                true
            );
            SDL_EndGPUCopyPass(copy_pass);


            SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(
                cmdbuf,
                &(SDL_GPUColorTargetInfo){
                    .texture = swapchain_texture,
                    .cycle = false,
                    .load_op = SDL_GPU_LOADOP_CLEAR,
                    .store_op = SDL_GPU_STOREOP_STORE,
                    .clear_color = (SDL_FColor){0.0f, 0.5f, 0.0f, 1.0f},
                },
                1,
                NULL
            );

            SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

            SDL_BindGPUFragmentSamplers(
                render_pass,
                0,
                &(SDL_GPUTextureSamplerBinding){
                    .texture = font.texture.handle,
                    /* .texture = texture.handle, */
                    .sampler = sampler,
                },
                1
            );

            SDL_BindGPUVertexStorageBuffers(render_pass, 0, &vertex_data_buffer, 1);
            SDL_PushGPUVertexUniformData(cmdbuf, 0, &(Vec2){800.0f, 600.0f}, sizeof(Vec2));
            SDL_PushGPUFragmentUniformData(cmdbuf, 0, &(Vec2){800.0f, 600.0f}, sizeof(Vec2));

            SDL_DrawGPUPrimitives(render_pass, store.size * 6, 1, 0, 0);
            SDL_EndGPURenderPass(render_pass);

        }

        SDL_SubmitGPUCommandBuffer(cmdbuf);
    }

	SDL_ReleaseGPUGraphicsPipeline(gpu, pipeline);
	SDL_ReleaseGPUSampler(gpu, sampler);
	SDL_ReleaseGPUTexture(gpu, texture.handle);
	SDL_ReleaseGPUTransferBuffer(gpu, vertex_data_transfer_buffer);
	SDL_ReleaseGPUBuffer(gpu, vertex_data_buffer);
    SDL_DestroyGPUDevice(gpu);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
