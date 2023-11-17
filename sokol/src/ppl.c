
#define SOKOL_IMPL

#if defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__linux) || defined(__unix__)
#define SOKOL_GLCORE33
#endif

/* #include "sokol_app.h" */
/* #include "sokol_gfx.h" */
/* #include "sokol_glue.h" */

#include "ppl.h"

#include "shape.glsl.h"

/* application state */
static struct {
    sg_pipeline pip;
    sg_bindings bind;
    sg_pass_action pass_action;
    float window_width;
    float window_height;
    float pos_x;
    float pos_y;
} state;


static float gl_x(float x) {
    return -1.0f + 2.0f * x / state.window_width;
}

static float gl_y(float y) {
    return 1.0f - 2.0f * y / state.window_height;
}


void ppl_init() {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });

    /* create shader from code-generated sg_shader_desc */
    sg_shader shd = sg_make_shader(simple_shader_desc(sg_query_backend()));

    /* a vertex buffer with 3 vertices */
    float vertices[] = {
        // positions
        state.pos_x + -0.5f, state.pos_y + -0.5f, 0.0f,     // bottom left
        state.pos_x + 0.5f, state.pos_y + -0.5f, 0.0f,      // bottom right
        state.pos_x + 0.0f,  state.pos_y + 0.5f, 0.0f       // top
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(vertices),
        .usage = SG_USAGE_DYNAMIC,
        .label = "shape-vertices"
    });

    /* create a pipeline object (default render states are fine for triangle) */
    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        /* if the vertex layout doesn't have gaps, don't need to provide strides and offsets */
        .layout = {
            .attrs = {
                [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3
            }
        },
        .label = "shape-pipeline"
    });

    /* a pass action to clear framebuffer */
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action=SG_LOADACTION_CLEAR, .clear_value={0.2f, 0.3f, 0.3f, 1.0f} }
    };

    state.window_width = 800.0f;
    state.window_height = 600.0f;
    state.pos_x = 0.0f;
    state.pos_y = 0.0f;
}

void ppl_draw() {
    /* a vertex buffer with 3 vertices */
    float vertices[] = {
        // positions
        state.pos_x + -0.5f, state.pos_y + -0.5f, 0.0f,     // bottom left
        state.pos_x + 0.5f, state.pos_y + -0.5f, 0.0f,      // bottom right
        state.pos_x + 0.0f,  state.pos_y + 0.5f, 0.0f       // top
    };
    sg_update_buffer(state.bind.vertex_buffers[0], &SG_RANGE(vertices));

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
}

void ppl_quit() {
    sg_shutdown();
}

void ppl_handle_event(sapp_event *e) {
    if (e->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (e->key_code == SAPP_KEYCODE_ESCAPE) {
            sapp_request_quit();
        }
    } else if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        /* state.pos_x = gl_x(e->mouse_x); */
        /* state.pos_y = gl_y(e->mouse_y); */
    } else if (e->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
        printf("scroll %f\n", e->scroll_y);
        state.pos_y -= e->scroll_y / state.window_height * 10.0;
    }
}
