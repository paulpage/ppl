package pgfx

import "core:strings"
import SDL "vendor:sdl2"
import gl "vendor:OpenGL"

MouseButtonState :: struct {
    down: bool,
    pressed: bool,
    clicks: i32,
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