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

#define TEXT_BUF_LEN 32
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

    SDL_AudioStream *stream;

    struct {
        Vec2 mouse;
        Vec2 wheel;
        bool buttons_down[BUTTON_COUNT];
        bool buttons_pressed[BUTTON_COUNT];
        bool buttons_released[BUTTON_COUNT];
        bool keys_down[KEY_COUNT];
        bool keys_pressed[KEY_COUNT];
        bool keys_released[KEY_COUNT];
        Key keymap[SDL_SCANCODE_COUNT];
        char textbuf[TEXT_BUF_LEN];
        int textbuf_pos;
    } input;
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

void sdl_init_keymap() {
    _APP.input.keymap[SDL_SCANCODE_SPACE] = KEY_SPACE;
    _APP.input.keymap[SDL_SCANCODE_APOSTROPHE] = KEY_APOSTROPHE;
    _APP.input.keymap[SDL_SCANCODE_COMMA] = KEY_COMMA;
    _APP.input.keymap[SDL_SCANCODE_MINUS] = KEY_MINUS;
    _APP.input.keymap[SDL_SCANCODE_PERIOD] = KEY_PERIOD;
    _APP.input.keymap[SDL_SCANCODE_SLASH] = KEY_SLASH;
    _APP.input.keymap[SDL_SCANCODE_0] = KEY_0;
    _APP.input.keymap[SDL_SCANCODE_1] = KEY_1;
    _APP.input.keymap[SDL_SCANCODE_2] = KEY_2;
    _APP.input.keymap[SDL_SCANCODE_3] = KEY_3;
    _APP.input.keymap[SDL_SCANCODE_4] = KEY_4;
    _APP.input.keymap[SDL_SCANCODE_5] = KEY_5;
    _APP.input.keymap[SDL_SCANCODE_6] = KEY_6;
    _APP.input.keymap[SDL_SCANCODE_7] = KEY_7;
    _APP.input.keymap[SDL_SCANCODE_8] = KEY_8;
    _APP.input.keymap[SDL_SCANCODE_9] = KEY_9;
    _APP.input.keymap[SDL_SCANCODE_SEMICOLON] = KEY_SEMICOLON;
    _APP.input.keymap[SDL_SCANCODE_EQUALS] = KEY_EQUAL;
    _APP.input.keymap[SDL_SCANCODE_A] = KEY_A;
    _APP.input.keymap[SDL_SCANCODE_B] = KEY_B;
    _APP.input.keymap[SDL_SCANCODE_C] = KEY_C;
    _APP.input.keymap[SDL_SCANCODE_D] = KEY_D;
    _APP.input.keymap[SDL_SCANCODE_E] = KEY_E;
    _APP.input.keymap[SDL_SCANCODE_F] = KEY_F;
    _APP.input.keymap[SDL_SCANCODE_G] = KEY_G;
    _APP.input.keymap[SDL_SCANCODE_H] = KEY_H;
    _APP.input.keymap[SDL_SCANCODE_I] = KEY_I;
    _APP.input.keymap[SDL_SCANCODE_J] = KEY_J;
    _APP.input.keymap[SDL_SCANCODE_K] = KEY_K;
    _APP.input.keymap[SDL_SCANCODE_L] = KEY_L;
    _APP.input.keymap[SDL_SCANCODE_M] = KEY_M;
    _APP.input.keymap[SDL_SCANCODE_N] = KEY_N;
    _APP.input.keymap[SDL_SCANCODE_O] = KEY_O;
    _APP.input.keymap[SDL_SCANCODE_P] = KEY_P;
    _APP.input.keymap[SDL_SCANCODE_Q] = KEY_Q;
    _APP.input.keymap[SDL_SCANCODE_R] = KEY_R;
    _APP.input.keymap[SDL_SCANCODE_S] = KEY_S;
    _APP.input.keymap[SDL_SCANCODE_T] = KEY_T;
    _APP.input.keymap[SDL_SCANCODE_U] = KEY_U;
    _APP.input.keymap[SDL_SCANCODE_V] = KEY_V;
    _APP.input.keymap[SDL_SCANCODE_W] = KEY_W;
    _APP.input.keymap[SDL_SCANCODE_X] = KEY_X;
    _APP.input.keymap[SDL_SCANCODE_Y] = KEY_Y;
    _APP.input.keymap[SDL_SCANCODE_Z] = KEY_Z;
    _APP.input.keymap[SDL_SCANCODE_LEFTBRACKET] = KEY_LEFT_BRACKET;
    _APP.input.keymap[SDL_SCANCODE_BACKSLASH] = KEY_BACKSLASH;
    _APP.input.keymap[SDL_SCANCODE_RIGHTBRACKET] = KEY_RIGHT_BRACKET;
    _APP.input.keymap[SDL_SCANCODE_GRAVE] = KEY_GRAVE_ACCENT;
    /* _APP.input.keymap[] = SDL_SCANCODE_UNKNOWNTODOKEY_WORLD_1;  // */ 
    /* APP.input.keymap[KEY_WORLD_2] = SDL_SCANCODE_UNKNOWN; // TODO_ */
    _APP.input.keymap[SDL_SCANCODE_ESCAPE] = KEY_ESCAPE;
    _APP.input.keymap[SDL_SCANCODE_RETURN] = KEY_ENTER;
    _APP.input.keymap[SDL_SCANCODE_TAB] = KEY_TAB;
    _APP.input.keymap[SDL_SCANCODE_BACKSPACE] = KEY_BACKSPACE;
    _APP.input.keymap[SDL_SCANCODE_INSERT] = KEY_INSERT;
    _APP.input.keymap[SDL_SCANCODE_DELETE] = KEY_DELETE;
    _APP.input.keymap[SDL_SCANCODE_RIGHT] = KEY_RIGHT;
    _APP.input.keymap[SDL_SCANCODE_LEFT] = KEY_LEFT;
    _APP.input.keymap[SDL_SCANCODE_DOWN] = KEY_DOWN;
    _APP.input.keymap[SDL_SCANCODE_UP] = KEY_UP;
    _APP.input.keymap[SDL_SCANCODE_PAGEUP] = KEY_PAGE_UP;
    _APP.input.keymap[SDL_SCANCODE_PAGEDOWN] = KEY_PAGE_DOWN;
    _APP.input.keymap[SDL_SCANCODE_HOME] = KEY_HOME;
    _APP.input.keymap[SDL_SCANCODE_END] = KEY_END;
    _APP.input.keymap[SDL_SCANCODE_CAPSLOCK] = KEY_CAPS_LOCK;
    _APP.input.keymap[SDL_SCANCODE_SCROLLLOCK] = KEY_SCROLL_LOCK;
    _APP.input.keymap[SDL_SCANCODE_NUMLOCKCLEAR] = KEY_NUM_LOCK;
    _APP.input.keymap[SDL_SCANCODE_PRINTSCREEN] = KEY_PRINT_SCREEN;
    _APP.input.keymap[SDL_SCANCODE_PAUSE] = KEY_PAUSE;
    _APP.input.keymap[SDL_SCANCODE_F1] = KEY_F1;
    _APP.input.keymap[SDL_SCANCODE_F2] = KEY_F2;
    _APP.input.keymap[SDL_SCANCODE_F3] = KEY_F3;
    _APP.input.keymap[SDL_SCANCODE_F4] = KEY_F4;
    _APP.input.keymap[SDL_SCANCODE_F5] = KEY_F5;
    _APP.input.keymap[SDL_SCANCODE_F6] = KEY_F6;
    _APP.input.keymap[SDL_SCANCODE_F7] = KEY_F7;
    _APP.input.keymap[SDL_SCANCODE_F8] = KEY_F8;
    _APP.input.keymap[SDL_SCANCODE_F9] = KEY_F9;
    _APP.input.keymap[SDL_SCANCODE_F10] = KEY_F10;
    _APP.input.keymap[SDL_SCANCODE_F11] = KEY_F11;
    _APP.input.keymap[SDL_SCANCODE_F12] = KEY_F12;
    _APP.input.keymap[SDL_SCANCODE_F13] = KEY_F13;
    _APP.input.keymap[SDL_SCANCODE_F14] = KEY_F14;
    _APP.input.keymap[SDL_SCANCODE_F15] = KEY_F15;
    _APP.input.keymap[SDL_SCANCODE_F16] = KEY_F16;
    _APP.input.keymap[SDL_SCANCODE_F17] = KEY_F17;
    _APP.input.keymap[SDL_SCANCODE_F18] = KEY_F18;
    _APP.input.keymap[SDL_SCANCODE_F19] = KEY_F19;
    _APP.input.keymap[SDL_SCANCODE_F20] = KEY_F20;
    _APP.input.keymap[SDL_SCANCODE_F21] = KEY_F21;
    _APP.input.keymap[SDL_SCANCODE_F22] = KEY_F22;
    _APP.input.keymap[SDL_SCANCODE_F23] = KEY_F23;
    _APP.input.keymap[SDL_SCANCODE_F24] = KEY_F24;
    /* _APP.input.keymap[] = SDL_SCANCODE_UNKNOWNTODOKEY_F25; // */ 
    _APP.input.keymap[SDL_SCANCODE_KP_0] = KEY_KP_0;
    _APP.input.keymap[SDL_SCANCODE_KP_1] = KEY_KP_1;
    _APP.input.keymap[SDL_SCANCODE_KP_2] = KEY_KP_2;
    _APP.input.keymap[SDL_SCANCODE_KP_3] = KEY_KP_3;
    _APP.input.keymap[SDL_SCANCODE_KP_4] = KEY_KP_4;
    _APP.input.keymap[SDL_SCANCODE_KP_5] = KEY_KP_5;
    _APP.input.keymap[SDL_SCANCODE_KP_6] = KEY_KP_6;
    _APP.input.keymap[SDL_SCANCODE_KP_7] = KEY_KP_7;
    _APP.input.keymap[SDL_SCANCODE_KP_8] = KEY_KP_8;
    _APP.input.keymap[SDL_SCANCODE_KP_9] = KEY_KP_9;
    _APP.input.keymap[SDL_SCANCODE_KP_DECIMAL] = KEY_KP_DECIMAL;
    _APP.input.keymap[SDL_SCANCODE_KP_DIVIDE] = KEY_KP_DIVIDE;
    _APP.input.keymap[SDL_SCANCODE_KP_MULTIPLY] = KEY_KP_MULTIPLY;
    _APP.input.keymap[SDL_SCANCODE_KP_MINUS] = KEY_KP_SUBTRACT;
    /* _APP.input.keymap[] = SDL_SCANCODE_UNKNOWN; /KEY_KP_ADD/ TODO */  
    _APP.input.keymap[SDL_SCANCODE_KP_ENTER] = KEY_KP_ENTER;
    _APP.input.keymap[SDL_SCANCODE_KP_EQUALS] = KEY_KP_EQUAL;
    _APP.input.keymap[SDL_SCANCODE_LSHIFT] = KEY_LEFT_SHIFT;
    _APP.input.keymap[SDL_SCANCODE_LCTRL] = KEY_LEFT_CONTROL;
    _APP.input.keymap[SDL_SCANCODE_LALT] = KEY_LEFT_ALT;
    _APP.input.keymap[SDL_SCANCODE_LGUI] = KEY_LEFT_SUPER;
    _APP.input.keymap[SDL_SCANCODE_RSHIFT] = KEY_RIGHT_SHIFT;
    _APP.input.keymap[SDL_SCANCODE_RCTRL] = KEY_RIGHT_CONTROL;
    _APP.input.keymap[SDL_SCANCODE_RALT] = KEY_RIGHT_ALT;
    _APP.input.keymap[SDL_SCANCODE_RGUI] = KEY_RIGHT_SUPER;
    _APP.input.keymap[SDL_SCANCODE_MENU] = KEY_MENU;
}

void app_init() {

    ASSERT_CALL(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));

    sdl_init_keymap();

    printf("Path: %s\n", SDL_GetBasePath());

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
    _APP.should_quit = true;
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

void sdl_process_events() {
    SDL_Event e;

    _APP.input.textbuf_pos = 0;
    for (int i = 0; i < KEY_COUNT; i++) {
        _APP.input.keys_pressed[i] = false;
        _APP.input.keys_released[i] = false;
    }
    for (int i = 0; i < TEXT_BUF_LEN; i++) {
        _APP.input.textbuf[i] = '\0';
    }
    for (int i = 0; i < BUTTON_COUNT; i++) {
        _APP.input.buttons_pressed[i] = false;
        _APP.input.buttons_released[i] = false;
    }

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                {
                    _APP.should_quit = true;
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                {

                    Button button = BUTTON_INVALID;
                    switch (e.button.button) {
                        case SDL_BUTTON_LEFT: button = BUTTON_LEFT; break;
                        case SDL_BUTTON_RIGHT: button = BUTTON_RIGHT; break;
                        case SDL_BUTTON_MIDDLE: button = BUTTON_MIDDLE; break;
                    }
                    _APP.input.buttons_down[button] = true;
                    _APP.input.buttons_pressed[button] = true;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    Button button = BUTTON_INVALID;
                    switch (e.button.button) {
                        case SDL_BUTTON_LEFT: button = BUTTON_LEFT; break;
                        case SDL_BUTTON_RIGHT: button = BUTTON_RIGHT; break;
                        case SDL_BUTTON_MIDDLE: button = BUTTON_MIDDLE; break;
                    }
                    _APP.input.buttons_down[button] = false;
                    _APP.input.buttons_released[button] = true;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                {
                    Key key = _APP.input.keymap[e.key.scancode];
                    _APP.input.keys_down[key] = true;
                    _APP.input.keys_pressed[key] = true;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                {
                    _APP.input.mouse.x = (f32)e.motion.x;
                    _APP.input.mouse.y = (f32)e.motion.y;
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                {
                    _APP.input.wheel.x = (f32)e.wheel.x;
                    _APP.input.wheel.y = (f32)e.wheel.y;
                }
                break;
            case SDL_EVENT_KEY_UP:
                {
                    Key key = _APP.input.keymap[e.key.scancode];
                    _APP.input.keys_down[key] = false;
                    _APP.input.keys_released[key] = true;
                }
                break;
            case SDL_EVENT_TEXT_INPUT:
                {
                    int i = 0;
                    while (e.text.text[i] && i < TEXT_BUF_LEN) {
                        _APP.input.textbuf[i] = e.text.text[i];
                        i++;
                    }
                }
                break;
        }
    }

}

bool app_should_quit() {
    sdl_flush();
    sdl_end_frame();

    sdl_process_events();

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

bool is_mouse_down(Button button) {
    return _APP.input.buttons_down[button];
}

bool is_mouse_pressed(Button button) {
    return _APP.input.buttons_pressed[button];
}

bool is_mouse_released(Button button) {
    return _APP.input.buttons_released[button];
}

bool is_key_pressed(Key key) {
    return _APP.input.keys_pressed[key];
}

bool is_key_down(Key key) {
    return _APP.input.keys_down[key];
}

bool is_key_released(Key key) {
    return _APP.input.keys_released[key];
}

char get_char() {
    char c = _APP.input.textbuf[_APP.input.textbuf_pos];
    if (_APP.input.textbuf_pos < TEXT_BUF_LEN - 1) {
        _APP.input.textbuf_pos++;
    }
    return c;
}
