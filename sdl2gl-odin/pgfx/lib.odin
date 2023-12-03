package pgfx

import "core:fmt"
import "core:os"
import "core:strings"
import "core:math"
import "core:runtime"
import "core:image"
import "core:image/png"
import SDL "vendor:sdl2"
import gl "vendor:OpenGL"

MouseButtonState :: struct {
    down: bool,
    pressed: bool,
    clicks: i32,
}

State :: struct {
    window: ^SDL.Window,
    gl_context: SDL.GLContext,
    program_2d: u32,
    uniforms_2d: map[string]gl.Uniform_Info,

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

    window_size_changed: bool,
    window_width: f32,
    window_height: f32,

    verts: [dynamic]f32,
}

GL_VERSION_MAJOR :: 4
GL_VERSION_MINOR :: 3

@(private)
state: State

Rect :: struct {
    x, y, w, h: f32,
}

Texture :: struct {
    id: u32,
    w, h: f32,
}

gl_debug_proc :: proc "c" (source: u32, type: u32, id: u32, severity: u32, length: i32, message: cstring, userParam: rawptr) {
    context = runtime.default_context()
    fmt.printf("OPENGL ERROR: %v\n", message)
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
        os.exit(1)
    }


    state.uniforms_2d = gl.get_uniforms_from_program(state.program_2d)

    state.window_width = 800
    state.window_height = 600
}

clear_background :: proc(color: [4]f32) {
    gl.ClearColor(color.r, color.g, color.b, color.a)
    gl.Clear(gl.COLOR_BUFFER_BIT)
}

resize :: proc(x: i32, y: i32) {
    state.window_width = f32(x)
    state.window_height = f32(y)
    gl.Viewport(0, 0, x, y)
}

update :: proc() -> bool {

    state.wheel = {}
    clear(&state.keys_pressed)
    clear(&state.physical_keys_pressed)
    strings.builder_reset(&state.text_entered)
    state.mouse_left.pressed = false
    state.mouse_middle.pressed = false
    state.mouse_right.pressed = false
    state.window_size_changed = false

    e: SDL.Event
    for SDL.PollEvent(&e) {
        #partial switch e.type {
            case .QUIT: return false
            case .WINDOWEVENT: {
                #partial switch e.window.event {
                    case .RESIZED: {
                        state.window_size_changed = true
                        resize(e.window.data1, e.window.data2)
                    }
                }
            }
            case .MOUSEBUTTONDOWN: {
                switch e.button.button {
                    case SDL.BUTTON_LEFT: {
                        state.mouse_left.down = true
                        state.mouse_left.pressed = true
                    }
                    case SDL.BUTTON_MIDDLE: {
                        state.mouse_middle.down = true
                        state.mouse_middle.pressed = true
                    }
                    case SDL.BUTTON_RIGHT: {
                        state.mouse_right.down = true
                        state.mouse_right.pressed = true
                    }
                }
            }
            case .MOUSEBUTTONUP: {
                switch  e.button.button {
                    case SDL.BUTTON_LEFT: state.mouse_left.down = false
                    case SDL.BUTTON_MIDDLE: state.mouse_middle.down = false
                    case SDL.BUTTON_RIGHT: state.mouse_right.down = false
                }
            }
            case .MOUSEMOTION: {
                state.mouse = {f32(e.motion.x), f32(e.motion.y)}
            }
            case .MOUSEWHEEL: {
                // TODO preciseX and preciseY
                state.wheel += {f32(e.wheel.x), f32(e.wheel.y)}
            }
            case .KEYDOWN: {
                state.keymod = e.key.keysym.mod
                state.keys_down[e.key.keysym.sym] = true
                state.physical_keys_down[e.key.keysym.scancode] = true
                state.keys_pressed[e.key.keysym.sym] = true
                state.physical_keys_pressed[e.key.keysym.scancode] = true
            }
            case .KEYUP: {
                state.keymod = e.key.keysym.mod
                state.keys_down[e.key.keysym.sym] = false
                state.physical_keys_down[e.key.keysym.scancode] = false
            }
            case .TEXTINPUT: {
                for c in e.text.text {
                    if c == 0 {
                        break
                    }
                    strings.write_byte(&state.text_entered, c)
                }
            }
        }
    }
    return true
}

present :: proc() {
    SDL.GL_SwapWindow(state.window)
}

_gl_coord :: proc(p: [2]f32) -> [2]f32 {
    return {
        p.x * 2 / state.window_width - 1.0,
        1 - p.y * 2 / state.window_height,
    }
}

_get_rect_vertices :: proc(rect: Rect, origin: [2]f32, rotation: f32) -> [4][2]f32 {
    x, y := rect.x, rect.y
    w, h := rect.w, rect.h
    dx, dy := -origin.x, -origin.y

    p1, p2, p3, p4: [2]f32
    if rotation == 0 {
        x += dx
        y += dy
        p1 = {x, y}
        p2 = {x + w, y}
        p3 = {x, y + h}
        p4 = {x + w, y + h}
    } else {
        rcos := math.cos(rotation)
        rsin := math.sin(rotation)
        p1 = {x + dx*rcos - dy*rsin, y + dx*rsin + dy*rcos}
        p2 = {x + (dx + w)*rcos - dy*rsin, y + (dx + w)*rsin + dy*rcos}
        p3 = {x + dx*rcos - (dy + h)*rsin, y + dx*rsin + (dy + h)*rcos}
        p4 = {x + (dx + w)*rcos - (dy + h)*rsin, y + (dx + w)*rsin + (dy + h)*rcos}
    }

    return {_gl_coord(p1), _gl_coord(p2), _gl_coord(p3), _gl_coord(p4)}
}

load_texture :: proc(filename: string) -> Texture {
    tex: Texture
    img, err := png.load(filename)
    defer image.destroy(img)
    if err != image.General_Image_Error.None {
        fmt.printf("error: %v", err)
    }
    tex.w = f32(img.width)
    tex.h = f32(img.height)

    fmt.printf("image %v\n", img)
    if img.depth != 8 {
        // TODO
        fmt.eprintln("Only image depth of 8 supported")
        return tex
    }

    gl.ActiveTexture(gl.TEXTURE0)
    gl.GenTextures(1, &tex.id)
    gl.BindTexture(gl.TEXTURE_2D, tex.id)
    gl.TexImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RGBA,
        i32(img.width),
        i32(img.height),
        0,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        raw_data(img.pixels.buf)
    )

    return tex
}

draw_texture :: proc(tex: Texture, src: Rect, dst: Rect, origin: [2]f32, rotation: f32, color: [4]f32) {
    p := _get_rect_vertices(dst, origin, rotation)

    u0 := src.x / tex.w
    v0 := (src.y + src.h) / tex.h
    u1 := (src.x + src.w) / tex.w
    v1 := src.y / tex.h
    
    new_verts: []f32 = {
        p[0].x, p[0].y, u0, v1,
        p[1].x, p[1].y, u1, v1,
        p[3].x, p[3].y, u1, v0,
        p[0].x, p[0].y, u0, v1,
        p[3].x, p[3].y, u1, v0,
        p[2].x, p[2].y, u0, v0,
        // p[0].x, p[0].y, u0, v1, color.r, color.g, color.b, color.a,
        // p[1].x, p[1].y, u1, v1, color.r, color.g, color.b, color.a,
        // p[3].x, p[3].y, u1, v0, color.r, color.g, color.b, color.a,
        // p[0].x, p[0].y, u0, v1, color.r, color.g, color.b, color.a,
        // p[3].x, p[3].y, u1, v0, color.r, color.g, color.b, color.a,
        // p[2].x, p[2].y, u0, v0, color.r, color.g, color.b, color.a,
    }

    append(&state.verts, ..new_verts)
    flush(tex.id)
}

flush :: proc(texture_id: u32) {
    vao, vbo: u32
    gl.ActiveTexture(gl.TEXTURE0)
    gl.BindTexture(gl.TEXTURE_2D, texture_id)
    gl.BlendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)
    gl.Disable(gl.DEPTH_TEST)

    // TODO Decide what these should be.
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE)
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE)

    gl.GenVertexArrays(1, &vao)
    gl.GenBuffers(1, &vbo)
    gl.BindBuffer(gl.ARRAY_BUFFER, vbo)
    gl.BufferData(
        gl.ARRAY_BUFFER,
        len(state.verts) * size_of(f32),
        raw_data(state.verts),
        gl.STATIC_DRAW
    )
    gl.BindVertexArray(vao)
    stride := 4 * size_of(f32)

    gl.EnableVertexAttribArray(0)
    gl.VertexAttribPointer(0, 2, gl.FLOAT, gl.FALSE, i32(stride), 0)
    gl.EnableVertexAttribArray(1)
    gl.VertexAttribPointer(1, 2, gl.FLOAT, gl.FALSE, i32(stride), 2 * size_of(f32))

    uniform := gl.GetUniformLocation(state.program_2d, "tex")
    gl.UseProgram(state.program_2d)
    gl.Uniform1i(uniform, 0)

    gl.DrawArrays(gl.TRIANGLES, 0, i32(len(state.verts)) / 4)

    gl.BindBuffer(gl.ARRAY_BUFFER, 0)
    gl.BindVertexArray(0)

    gl.DeleteBuffers(1, &vbo)
    gl.DeleteVertexArrays(1, &vao)
    // gl.DeleteTextures(1, &id)

    // fmt.printf("verts: %v\n", state.verts)

    clear(&state.verts)
}

quit :: proc() {
    delete(state.uniforms_2d)
    gl.DeleteProgram(state.program_2d)
	SDL.GL_DeleteContext(state.gl_context)
    SDL.DestroyWindow(state.window)
    SDL.Quit()
}

mouse_pos :: proc() -> [2]f32 {
    return state.mouse
}

mouse_wheel :: proc() -> [2]f32 {
    return state.wheel
}

text_entered :: proc() -> string {
    return strings.to_string(state.text_entered)
}
