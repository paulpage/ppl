package ui

push_color :: proc(color: [4]f32) {
    style := state.styles[len(state.styles) - 1]
    style.color_background = color
    append(&state.styles, style)
}

pop_color :: proc() {
    pop(&state.styles)
}
