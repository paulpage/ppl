#include "platform.h"

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

#define ASSERT_CALL(call) \
    do { \
        if (!(call)) { \
            SDL_Log("Error in %s: %s", #call, SDL_GetError()); \
            SDL_Quit(); \
            exit(1); \
        } \
    } while (0)

#define ASSERT_CREATED(obj) do { if ((obj) == NULL) { SDL_Log("Error: %s is null", #obj); SDL_Quit(); exit(1); }} while (0)

/* struct Texture { */
/*     SDL_GPUTexture *handle; */
/*     int w, h, d; */
/* }; */

#define FONT_SIZE 24.0f
#define ATLAS_WIDTH 512
#define ATLAS_HEIGHT 512

typedef struct VertInput {
    Rect dst_rect;
    Rect src_rect;
    Color border_color;
    Vec4 corner_radii;
    Color colors[4];
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

struct {
    AppConfig config;
    bool should_quit;
    SDL_GPUDevice *gpu;
    SDL_Window *window;
    SDL_GPUTransferBuffer *vertex_data_transfer_buffer;
    SDL_GPUBuffer *vertex_data_buffer;
    SDL_GPUGraphicsPipeline *pipeline;
    VertStore vertex_data_store;
    u64 buf_capacity;
    Texture last_texture;
    int texture_count;
    SDL_GPUSampler *sampler;
    Texture rect_texture;
    SDL_GPUCommandBuffer *cmdbuf;
    SDL_GPUTexture *swapchain_texture;
    SDL_GPURenderPass *render_pass;
} _APP = {0};


Texture load_texture_bytes(u8 *data, int w, int h, int d) {

    SDL_GPUTransferBuffer *texture_transfer_buffer = SDL_CreateGPUTransferBuffer(
        _APP.gpu,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = w * h * d,
        }
    );

    u8 *texture_transfer_ptr = SDL_MapGPUTransferBuffer(
        _APP.gpu,
        texture_transfer_buffer,
        false
    );
    SDL_memcpy(texture_transfer_ptr, data, w * h * d);
    SDL_UnmapGPUTransferBuffer(_APP.gpu, texture_transfer_buffer);

    SDL_GPUTexture *handle;
    if (d == 4) {
        handle = SDL_CreateGPUTexture(
            _APP.gpu,
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
            _APP.gpu,
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

    SDL_GPUCommandBuffer *upload_cmd_buf = SDL_AcquireGPUCommandBuffer(_APP.gpu);
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
    SDL_ReleaseGPUTransferBuffer(_APP.gpu, texture_transfer_buffer);

    int idx = _APP.texture_count;
    _APP.texture_count++;

    return (Texture){
        .handle = handle,
        .w = w,
        .h = h,
        .d = d,
        .idx = idx,
    };
}

Texture load_texture(char *filename) {

    int w, h, n;
    u8 *data = stbi_load(filename, &w, &h, &n, 0);
    ASSERT_CREATED(data);

    Texture texture = load_texture_bytes(data, w, h, n);

    stbi_image_free(data);
    return texture;
}

Font load_font(const char* font_path) {

    Font font = {0};
    font.char_data = malloc(96 * sizeof(stbtt_packedchar));
    font.scale = FONT_SIZE;
    size_t font_size = 0;
    u8 *font_buffer = os_read_file(font_path, &font_size);
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

    u8 *pixels = malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        pixels[i*4] = 0;
        pixels[i*4 + 1] = 0;
        pixels[i*4 + 2] = 0;
        pixels[i * 4 + 3] = atlas_data[i];
    }
    font.texture = load_texture_bytes(pixels, ATLAS_WIDTH, ATLAS_HEIGHT, 4);

	free(pixels);

    free(atlas_data);
    free(font_buffer);

    return font;
}

static SDL_GPUShader *sdl_load_shader(
    SDL_GPUDevice *gpu,
    char *filename,
    SDL_GPUShaderStage stage,
    int num_samplers,
    int num_storage_textures,
    int num_storage_buffers,
    int num_uniform_buffers
) {
    size_t len;
    unsigned char *data = os_read_file(filename, &len);
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

void app_init() {

    char *title = "Application";
    int width = 800;
    int height = 600;
    if (_APP.config.title) {
        title = _APP.config.title;
    }
    if (_APP.config.w) {
        width = _APP.config.w;
    }
    if (_APP.config.h) {
        height = _APP.config.h;
    }

    _APP.window = SDL_CreateWindow(title, width, height, 0);
    ASSERT_CREATED(_APP.window);

    _APP.gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    ASSERT_CREATED(_APP.gpu);
    ASSERT_CALL(SDL_ClaimWindowForGPUDevice(_APP.gpu, _APP.window));

    SDL_GPUShader *vertex_shader = sdl_load_shader(_APP.gpu, "shaders/2d.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 1, 1);
    SDL_GPUShader *fragment_shader = sdl_load_shader(_APP.gpu, "shaders/2d.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 1);

    // Pipeline
    _APP.pipeline = SDL_CreateGPUGraphicsPipeline(
		_APP.gpu,
		&(SDL_GPUGraphicsPipelineCreateInfo){
			.target_info = (SDL_GPUGraphicsPipelineTargetInfo){
				.num_color_targets = 1,
				.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
					.format = SDL_GetGPUSwapchainTextureFormat(_APP.gpu, _APP.window),
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
    ASSERT_CREATED(_APP.pipeline);

    SDL_ReleaseGPUShader(_APP.gpu, vertex_shader);
    SDL_ReleaseGPUShader(_APP.gpu, fragment_shader);

    // Textures

    _APP.sampler = SDL_CreateGPUSampler(
        _APP.gpu,
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
    _APP.vertex_data_store = make_vert_store();

    _APP.vertex_data_transfer_buffer = SDL_CreateGPUTransferBuffer(
        _APP.gpu,
        &(SDL_GPUTransferBufferCreateInfo){
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = _APP.vertex_data_store.capacity * sizeof(VertInput),
        }
    );

    _APP.vertex_data_buffer = SDL_CreateGPUBuffer(
        _APP.gpu,
        &(SDL_GPUBufferCreateInfo){
            .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
            .size = _APP.vertex_data_store.capacity * sizeof(VertInput),
        }
    );

    _APP.buf_capacity = _APP.vertex_data_store.capacity;

    u8 bytes[4] = {0, 0, 0, 0};
    _APP.rect_texture = load_texture_bytes(bytes, 1, 1, 4);
    _APP.last_texture = _APP.rect_texture;
}

void app_quit() {
}

void sdl_begin_frame() {
    _APP.cmdbuf = SDL_AcquireGPUCommandBuffer(_APP.gpu);
    ASSERT_CREATED(_APP.cmdbuf);

    _APP.swapchain_texture = NULL;
    ASSERT_CALL(SDL_AcquireGPUSwapchainTexture(_APP.cmdbuf, _APP.window, &_APP.swapchain_texture, NULL, NULL));
}

void sdl_end_frame() {
    SDL_SubmitGPUCommandBuffer(_APP.cmdbuf);
}

void sdl_flush() {
    if (_APP.vertex_data_store.size == 0) {
        return;
    }

    if (_APP.buf_capacity != _APP.vertex_data_store.capacity) {
        SDL_ReleaseGPUTransferBuffer(_APP.gpu, _APP.vertex_data_transfer_buffer);
        _APP.vertex_data_transfer_buffer = SDL_CreateGPUTransferBuffer(
            _APP.gpu,
            &(SDL_GPUTransferBufferCreateInfo){
                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                .size = _APP.vertex_data_store.capacity * sizeof(VertInput),
            }
        );
        SDL_ReleaseGPUBuffer(_APP.gpu, _APP.vertex_data_buffer);
        _APP.vertex_data_buffer = SDL_CreateGPUBuffer(
            _APP.gpu,
            &(SDL_GPUBufferCreateInfo){
                .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                .size = _APP.vertex_data_store.capacity * sizeof(VertInput),
            }
        );
        _APP.buf_capacity = _APP.vertex_data_store.capacity;
    }

    if (_APP.swapchain_texture) {

        if (_APP.vertex_data_store.size > 0) {

            VertInput *data_ptr = SDL_MapGPUTransferBuffer(_APP.gpu, _APP.vertex_data_transfer_buffer, true);

            for (int i = 0; i < _APP.vertex_data_store.size; i++) {
                data_ptr[i] = _APP.vertex_data_store.data[i];
            }

            SDL_UnmapGPUTransferBuffer(_APP.gpu, _APP.vertex_data_transfer_buffer);

            SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(_APP.cmdbuf);
            SDL_UploadToGPUBuffer(
                    copy_pass,
                    &(SDL_GPUTransferBufferLocation) {
                    .transfer_buffer = _APP.vertex_data_transfer_buffer,
                    .offset = 0,
                    },
                    &(SDL_GPUBufferRegion) {
                    .buffer = _APP.vertex_data_buffer,
                    .offset = 0,
                    .size = _APP.vertex_data_store.size * sizeof(VertInput),
                    },
                    true
                    );
            SDL_EndGPUCopyPass(copy_pass);
        }

        _APP.render_pass = SDL_BeginGPURenderPass(
            _APP.cmdbuf,
            &(SDL_GPUColorTargetInfo){
                .texture = _APP.swapchain_texture,
                .cycle = false,
                .load_op = SDL_GPU_LOADOP_LOAD,
                .store_op = SDL_GPU_STOREOP_STORE,
            },
            1,
            NULL
        );

        SDL_BindGPUGraphicsPipeline(_APP.render_pass, _APP.pipeline);

        SDL_BindGPUFragmentSamplers(
                _APP.render_pass,
                0,
                &(SDL_GPUTextureSamplerBinding){
                .texture = _APP.last_texture.handle,
                .sampler = _APP.sampler,
                },
                1
                );

        SDL_BindGPUVertexStorageBuffers(_APP.render_pass, 0, &_APP.vertex_data_buffer, 1);
        SDL_PushGPUVertexUniformData(_APP.cmdbuf, 0, &(Vec2){800.0f, 600.0f}, sizeof(Vec2));
        SDL_PushGPUFragmentUniformData(_APP.cmdbuf, 0, &(Vec2){800.0f, 600.0f}, sizeof(Vec2));

        SDL_DrawGPUPrimitives(_APP.render_pass, _APP.vertex_data_store.size * 6, 1, 0, 0);
        SDL_EndGPURenderPass(_APP.render_pass);
    }

    _APP.vertex_data_store.size = 0;
}

bool app_should_quit() {
    sdl_flush();
    sdl_end_frame();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                _APP.should_quit = true;
                break;
        }
    }

    sdl_begin_frame();
    return _APP.should_quit;
}

void app_clear(Color color) {
    if (_APP.swapchain_texture) {
        _APP.render_pass = SDL_BeginGPURenderPass(
            _APP.cmdbuf,
            &(SDL_GPUColorTargetInfo){
                .texture = _APP.swapchain_texture,
                .cycle = false,
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_STORE,
                .clear_color = (SDL_FColor){0.0f, 0.5f, 0.0f, 1.0f},
            },
            1,
            NULL
        );
        SDL_EndGPURenderPass(_APP.render_pass);

    }
}

void draw_rect(Rect rect, Color color) {
    if (_APP.last_texture.idx != _APP.rect_texture.idx || 1) {
        sdl_flush();
        _APP.last_texture = _APP.rect_texture;
    }

    VertInput vert = {
        .dst_rect = rect,
        .src_rect = (Rect){0.0f, 0.0f, 1.0f, 1.0f},
        .corner_radii = {0.0f, 0.0f, 0.0f, 0.0f},
        .border_color = color,
        .colors = {color, color, color, color},
        .edge_softness = 0.0f,
        .border_thickness = 0.0f,
        .use_texture = 0.0f,
    };

    push_vert(&_APP.vertex_data_store, vert);
}

void draw_texture(Texture *texture, Rect src, Rect dst) {
    if (_APP.last_texture.idx != texture->idx || 1) {
        sdl_flush();
        _APP.last_texture = *texture;
    }

    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    src.w = src.w / texture->w;
    src.h = src.h / texture->h;
    VertInput vert = {
        .dst_rect = dst,
        .src_rect = src,
        .corner_radii = {0.0f, 0.0f, 0.0f, 0.0f},
        .border_color = color,
        .colors = {color, color, color, color},
        .edge_softness = 0.0f,
        .border_thickness = 0.0f,
        .use_texture = 1.0f,
    };

    push_vert(&_APP.vertex_data_store, vert);
}

void draw_text(Font *font, const char *text, float x, float y) {
    if (_APP.last_texture.idx != font->texture.idx || 1) {
        sdl_flush();
        _APP.last_texture = font->texture;
    }

    /* u8 r, g, b, a; */
    y += font->scale;
    while (*text) {
        if (*text >= 32 && *text < 128) {
            stbtt_aligned_quad quad;
            stbtt_GetPackedQuad(font->char_data, ATLAS_WIDTH, ATLAS_HEIGHT, *text - 32, &x, &y, &quad, 1);

            VertInput vert = {
                .dst_rect = (Rect){quad.x0, quad.y0, quad.x1 - quad.x0, quad.y1 - quad.y0},
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
            push_vert(&_APP.vertex_data_store, vert);
        }
        text++;
    }
}
