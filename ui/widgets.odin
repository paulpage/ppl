package ui

button :: proc(name: string) -> Interaction {
    return check_widget(Widget{
        name = name,
        size = {
            {.TextContent, 0, 1},
            {.TextContent, 0, 1},
        },
        flags = {.Clickable, .DrawBorder, .DrawRect, .DrawText, .Hoverable}
    })
}

rect_button :: proc(name: string, width: f32, height: f32, color: [4]f32) -> Interaction {
    push_color(color)
    interaction := check_widget(Widget{
        name = name,
        size = {
            {.Pixels, width, 1},
            {.Pixels, height, 1},
        },
        flags = {.Clickable, .DrawBorder, .DrawRect, .Hoverable}
    })
    pop_color()
    return interaction
}

label :: proc(name: string) -> Interaction {
    return check_widget(Widget{
        name = name,
        size = {
            {.TextContent, 0, 1},
            {.TextContent, 0, 1},
        },
        flags = {.DrawText}
    })
}

spacer :: proc(name: string) -> Interaction {
    return check_widget(Widget{
        name = name,
        size = {
            {.PercentOfParent, 100, 0},
            {.PercentOfParent, 100, 0},
        },
        flags = {},
    })
}
