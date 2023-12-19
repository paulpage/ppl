package ui

button :: proc(name: string) -> Interaction {
    w := state.current_id
    id := uint(len(state.windows[w].widgets))

    widget: Widget
    widget.id = id
    widget.name = name
    widget.size = {
        {.TextContent, 0, 1},
        {.TextContent, 0, 1},
    }
    widget.flags = {.Clickable, .DrawBorder, .DrawRect, .DrawText, .Hoverable}

    _, interaction := check_widget(&state.windows[w], widget)
    return interaction
}

rect_button :: proc(name: string, width: f32, height: f32, color: [4]f32) -> Interaction {
    push_color(color)
    w := state.current_id
    id := uint(len(state.windows[w].widgets))

    widget: Widget
    widget.id = id
    widget.name = name
    widget.size = {
        {.Pixels, width, 1},
        {.Pixels, height, 1},
    }
    widget.flags = {.Clickable, .DrawBorder, .DrawRect, .Hoverable}

    _, interaction := check_widget(&state.windows[w], widget)
    pop_color()
    return interaction
}

label :: proc(name: string) -> Interaction {
    w := state.current_id
    id := uint(len(state.windows[w].widgets))

    widget := Widget{
        id = id,
        name = name,
        size = {
            {.TextContent, 0, 1},
            {.TextContent, 0, 1},
        },
        flags = {.DrawText}
    }
    _, interaction := check_widget(&state.windows[w], widget)
    return interaction
}

spacer :: proc(name: string) -> Interaction {
    w := state.current_id
    id := uint(len(state.windows[w].widgets))

    widget: Widget
    widget.id = id
    widget.name = name
    widget.size = {
        {.PercentOfParent, 100, 0},
        {.PercentOfParent, 100, 0},
    }
    widget.flags = {}

    _, interaction := check_widget(&state.windows[w], widget)
    return interaction
}
