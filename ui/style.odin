package ui

push_color :: proc(color: [4]f32) {
    style := state.styles[len(state.styles) - 1]
    style.color = color
    append(&state.styles, style)
}

push_border_color :: proc(color: [4]f32) {
    style := state.styles[len(state.styles) - 1]
    style.border_color = color
    append(&state.styles, style)
}

pop_style :: proc() {
    pop(&state.styles)
}

