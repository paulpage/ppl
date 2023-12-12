package pgfx

import "core:fmt"
import "core:runtime"
import "core:image"
import "core:image/png"
import "core:math"
import SDL "vendor:sdl2"
import gl "vendor:OpenGL"
import "vendor:fontstash"

GL_VERSION_MAJOR :: 3
GL_VERSION_MINOR :: 1

gl_debug_proc :: proc "c" (source: u32, type: u32, id: u32, severity: u32, length: i32, message: cstring, userParam: rawptr) {
    context = runtime.default_context()
    switch type {
        case gl.DEBUG_TYPE_ERROR: {
            fmt.print("GL_DEBUG ERROR: ")
        }
        case gl.DEBUG_TYPE_DEPRECATED_BEHAVIOR: {
            fmt.print("GL_DEBUG DEPRECATED: ")
        }
        case gl.DEBUG_TYPE_UNDEFINED_BEHAVIOR: {
            fmt.print("GL_DEBUG UNDEFINED ")
        }
        case gl.DEBUG_TYPE_PORTABILITY: {
            fmt.print("GL_DEBUG PORTABILITY: ")
        }
        case gl.DEBUG_TYPE_PERFORMANCE: {
            fmt.print("GL_DEBUG PERFORMANCE: ")
        }
        case gl.DEBUG_TYPE_MARKER: {
            fmt.print("GL_DEBUG MARKER: ")
        }
        case gl.DEBUG_TYPE_PUSH_GROUP: {
            fmt.print("GL_DEBUG PUSH_GROUP: ")
        }
        case gl.DEBUG_TYPE_POP_GROUP: {
            fmt.print("GL_DEBUG POP_GROUP: ")
        }
        case gl.DEBUG_TYPE_OTHER: {
            return
            // fmt.print("GL_DEBUG OTHER: ")
        }
    }
    fmt.println(message)
}

draw_text :: proc(text: string, x: f32, y: f32, size: f32, color: [4]f32) {
    fs := &state.font_context
    fontstash.BeginState(fs)
    fontstash.SetSize(fs, size)
    iter := fontstash.TextIterInit(fs, x, y, text)
    q: fontstash.Quad
    for fontstash.TextIterNext(fs, &iter, &q) {
        s0 := q.s0 * state.font_texture.w
        t0 := q.t0 * state.font_texture.h
        s1 := q.s1 * state.font_texture.w
        t1 := q.t1 * state.font_texture.h
        draw_texture_ex(state.font_texture, {s0, t0, s1-s0, t1-t0}, {q.x0, q.y0, q.x1-q.x0, q.y1-q.y0}, 0, 0, color, true)
    }
    fontstash.EndState(fs)
}

font_callback_resize :: proc(userdata: rawptr, w, h: int) {
    fmt.println("font_callback_resize\n")

    fs := &state.font_context
    state.font_texture.w = f32(fs.width)
    state.font_texture.h = f32(fs.height)
    gl.ActiveTexture(gl.TEXTURE0)
    gl.GenTextures(1, &state.font_texture.id)
    gl.BindTexture(gl.TEXTURE_2D, state.font_texture.id)
    gl.TexImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RED,
        i32(fs.width),
        i32(fs.height),
        0,
        gl.RED,
        gl.UNSIGNED_BYTE,
        raw_data(fs.textureData)
    )
}

font_callback_update :: proc(userdata: rawptr, dirtyRect: [4]f32, textureData: rawptr) {
    fmt.printf("font_callback_update %v\n", dirtyRect)

    fs := &state.font_context
    // TODO be more smart about dirty rect
    state.font_texture.w = f32(fs.width)
    state.font_texture.h = f32(fs.height)
    gl.ActiveTexture(gl.TEXTURE0)
    gl.GenTextures(1, &state.font_texture.id)
    gl.BindTexture(gl.TEXTURE_2D, state.font_texture.id)
    gl.TexImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RED,
        i32(fs.width),
        i32(fs.height),
        0,
        gl.RED,
        gl.UNSIGNED_BYTE,
        raw_data(fs.textureData)
    )
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

present :: proc() {
    flush(state.last_texture, state.last_is_text, true)
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
    if err != nil {
        fmt.eprintf("error: %v\n", err)
    }

    fmt.printf("image %v\n", img)
    if img.depth != 8 {
        // TODO
        fmt.eprintln("Only image depth of 8 supported")
        return tex
    }

    return load_texture_mem(i32(img.width), i32(img.height), img.pixels.buf[:])
}

load_texture_mem :: proc(w: i32, h: i32, data: []u8) -> Texture {
    tex: Texture
    tex.w = f32(w)
    tex.h = f32(h)

    gl.ActiveTexture(gl.TEXTURE0)
    gl.GenTextures(1, &tex.id)
    gl.BindTexture(gl.TEXTURE_2D, tex.id)
    gl.TexImage2D(
        gl.TEXTURE_2D,
        0,
        gl.RGBA,
        w,
        h,
        0,
        gl.RGBA,
        gl.UNSIGNED_BYTE,
        raw_data(data)
    )

    return tex
}

draw_texture :: proc(tex: Texture, src: Rect, dst: Rect) {
    draw_texture_ex(tex, src, dst, {0, 0}, 0, {1, 1, 1, 1}, false)
}

draw_texture_ex :: proc(tex: Texture, src: Rect, dst: Rect, origin: [2]f32, rotation: f32, color: [4]f32, is_text: bool) {
    flush(tex, is_text, false)

    p := _get_rect_vertices(dst, origin, rotation)

    u0 := src.x / tex.w
    v0 := (src.y + src.h) / tex.h
    u1 := (src.x + src.w) / tex.w
    v1 := src.y / tex.h
    
    new_verts: []f32 = {
        p[0].x, p[0].y, u0, v1, color.r, color.g, color.b, color.a,
        p[1].x, p[1].y, u1, v1, color.r, color.g, color.b, color.a,
        p[3].x, p[3].y, u1, v0, color.r, color.g, color.b, color.a,
        p[0].x, p[0].y, u0, v1, color.r, color.g, color.b, color.a,
        p[3].x, p[3].y, u1, v0, color.r, color.g, color.b, color.a,
        p[2].x, p[2].y, u0, v0, color.r, color.g, color.b, color.a,
    }

    append(&state.verts, ..new_verts)
}

draw_rect :: proc(rect: Rect, color: [4]f32) {
    draw_rect_ex(rect, {0, 0}, 0, color);
}

draw_rect_ex :: proc(rect: Rect, origin: [2]f32, rotation: f32, color: [4]f32) {
    draw_texture_ex(state.geometry_texture, {0, 0, 1, 1}, rect, origin, rotation, color, false)
}

flush :: proc(texture: Texture, is_text: bool, force: bool) {

    if force {
        state.last_texture = texture
        state.last_is_text = is_text
    } else if texture.id == state.last_texture.id && is_text == state.last_is_text {
        return
    }

   vao: u32
    gl.ActiveTexture(gl.TEXTURE0)
    gl.BindTexture(gl.TEXTURE_2D, state.last_texture.id)
    gl.BlendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA)
    gl.Disable(gl.DEPTH_TEST)

    // TODO Decide what these should be.
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE)
    gl.TexParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE)

    gl.GenVertexArrays(1, &vao)
    gl.BindBuffer(gl.ARRAY_BUFFER, state.vbo)
    if state.last_vert_len == len(state.verts) {
        gl.BufferSubData(
            gl.ARRAY_BUFFER,
            0,
            len(state.verts) * size_of(f32),
            raw_data(state.verts)
        )
    } else {
        gl.BufferData(
            gl.ARRAY_BUFFER,
            len(state.verts) * size_of(f32),
            raw_data(state.verts),
            gl.DYNAMIC_DRAW
        )
        state.last_vert_len = len(state.verts)
    }
    gl.BindVertexArray(vao)
    stride := 8 * size_of(f32)

    gl.EnableVertexAttribArray(0)
    gl.VertexAttribPointer(0, 2, gl.FLOAT, gl.FALSE, i32(stride), 0)
    gl.EnableVertexAttribArray(1)
    gl.VertexAttribPointer(1, 2, gl.FLOAT, gl.FALSE, i32(stride), 2 * size_of(f32))
    gl.EnableVertexAttribArray(2)
    gl.VertexAttribPointer(2, 4, gl.FLOAT, gl.FALSE, i32(stride), 4 * size_of(f32))

    uniform := gl.GetUniformLocation(state.program_2d, "tex")
    is_text_uniform := gl.GetUniformLocation(state.program_2d, "is_text")
    gl.UseProgram(state.program_2d)
    gl.Uniform1i(uniform, 0)
    gl.Uniform1i(is_text_uniform, i32(state.last_is_text))

    gl.DrawArrays(gl.TRIANGLES, 0, i32(len(state.verts)) / 4)

    gl.BindBuffer(gl.ARRAY_BUFFER, 0)
    gl.BindVertexArray(0)

    gl.DeleteVertexArrays(1, &vao)
    // gl.DeleteTextures(1, &id)

    clear(&state.verts)
    state.last_texture = texture
    state.last_is_text = is_text
}
