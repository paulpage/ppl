package pgfx

import "core:fmt"
import "core:os"
import "core:strings"
import SDL "vendor:sdl2"
import gl "vendor:OpenGL"
import "vendor:fontstash"

State :: struct {
    // Window Context
    window: ^SDL.Window,
    gl_context: SDL.GLContext,

    // Window Info
    window_size_changed: bool,
    window_width: f32,
    window_height: f32,

    // Input
    mouse: [2]f32,
    wheel: [2]f32,
    mouse_left: MouseButtonState,
    mouse_right: MouseButtonState,
    mouse_middle: MouseButtonState,
    keymod: SDL.Keymod,
    text_entered: strings.Builder,
    keys_down: map[SDL.Keycode]bool,
    keys_pressed: map[SDL.Keycode]bool,
    physical_keys_down: map[SDL.Scancode]bool,
    physical_keys_pressed: map[SDL.Scancode]bool,

    // Text
    font_context: fontstash.FontContext,

    // OpenGL
    program_2d: u32,
    program_text: u32,
    uniforms_2d: map[string]gl.Uniform_Info,
    uniforms_text: map[string]gl.Uniform_Info,
    geometry_texture: Texture,
    font_texture: Texture,
    last_texture: Texture,
    last_is_text: bool,
    last_vert_len: int,
    verts: [dynamic]f32,
    vbo: u32,
}

// @(private)
state: State

Rect :: struct {
    x, y, w, h: f32,
}

Texture :: struct {
    id: u32,
    w, h: f32,
}

init :: proc(title: string) {
    ok: bool

    SDL.Init({.VIDEO})
    state.window = SDL.CreateWindow(strings.clone_to_cstring(title), SDL.WINDOWPOS_UNDEFINED, SDL.WINDOWPOS_UNDEFINED, 800, 600, {.OPENGL, .RESIZABLE})
    if state.window == nil {
        fmt.eprintln("Failed to create window")
        os.exit(1)
    }

	SDL.GL_SetAttribute(.CONTEXT_PROFILE_MASK,  i32(SDL.GLprofile.CORE))
	SDL.GL_SetAttribute(.CONTEXT_MAJOR_VERSION, GL_VERSION_MAJOR)
	SDL.GL_SetAttribute(.CONTEXT_MINOR_VERSION, GL_VERSION_MINOR)

	state.gl_context = SDL.GL_CreateContext(state.window)

	gl.load_up_to(GL_VERSION_MAJOR, GL_VERSION_MINOR, SDL.gl_set_proc_address)


    gl.Enable(gl.DEBUG_OUTPUT)
    gl.Enable(gl.BLEND)
    gl.DebugMessageCallback(gl_debug_proc, nil)

    // TODO make sure these get loaded from anywhere, maybe #load
    state.program_2d, ok = gl.load_shaders("pgfx/shaders/2d_vert.glsl", "pgfx/shaders/2d_frag.glsl")
    if !ok {
        fmt.eprintln("Failed to create GLSL program")
        compile_msg, compile_type, link_msg, link_type := gl.get_last_error_messages()
        fmt.printf("%v %v %v %v\n", compile_msg, compile_type, link_msg, link_type)
        os.exit(1)
    }

    state.uniforms_2d = gl.get_uniforms_from_program(state.program_2d)
    gl.GenBuffers(1, &state.vbo)

    // Make a 1x1 texture for drawing geometry
    state.geometry_texture = load_texture_mem(1, 1, []u8{255, 255, 255, 255})

    state.window_width = 800
    state.window_height = 600

    state.font_context.callbackResize = font_callback_resize
    state.font_context.callbackUpdate = font_callback_update
    fs := &state.font_context
    fontstash.Init(fs, 512, 512, .TOPLEFT)
    // TODO let user select font
    font := fontstash.AddFont(&state.font_context, "sans", "../res/DroidSans.ttf")
    fontstash.SetFont(&state.font_context, font)

    // TODO temp testing
    fontstash.SetSize(fs, 20)

}

quit :: proc() {
    delete(state.uniforms_2d)
    gl.DeleteBuffers(1, &state.vbo)
    gl.DeleteProgram(state.program_2d)
	SDL.GL_DeleteContext(state.gl_context)
    SDL.DestroyWindow(state.window)
    SDL.Quit()
}

