package ui

import "core:math/rand"
import "core:fmt"

import app "../pgfx"

Rect :: app.Rect

SizeKind :: enum {
    Pixels,
    PercentOfParent,
    TextContent,
    ChildrenSum,
    ChildrenMax,
}

Size :: struct {
    kind: SizeKind,
    value: f32,
    strictness: f32,
}

WidgetFlag :: enum {
    DrawText,
    Clickable,
    DrawBorder,
    Movable,
}

WidgetFlags :: bit_set[WidgetFlag]

Interaction :: struct {
    clicked: bool,
    hovered: bool,
}

Layout :: enum {
    Floating,
    Horizontal,
    Vertical,
    ToolRow,
    ToolColumn,
}

Widget :: struct {
    // Tree
    id: uint,
    parent: uint,
    children: [dynamic]uint,

    // Basic info
    name: string,
    size: [2]Size,
    flags: WidgetFlags,
    layout: Layout,
    requested_pos: [2]f32,

    // State
    dragging: bool,
    hovered: bool,

    // Computed values
    computed_size: [2]f32,
    computed_rect: Rect,
    rect: Rect,
}

Window :: struct {
    name: string,
    rect: Rect,

    widgets: [dynamic]Widget,
    current_id: uint,

    mouse_intercepted: bool,
    zindex: uint,
}

StyleInfo :: struct {
    font_size: f32,
    border_size: f32,
    padding: f32,
    color_background: [4]f32,
    color_border: [4]f32,
    color_text: [4]f32,
    temp_colors: [dynamic][4]f32,
    temp_color_idx: uint,
}

Ui :: struct {
    // Tree
    windows: [dynamic]Window,
    current_id: uint,

    // Style
    style: StyleInfo,

    // State
    next_floating_window_pos: [2]f32,
    mouse_intercepted: bool,
}

calc_parent_dependent :: proc(window: ^Window, id: uint, level: uint) {
    for i in 0..<2 {
        #partial switch window.widgets[id].size[i].kind {
            case .PercentOfParent: {
                parent := window.widgets[id].parent
                percent := window.widgets[id].size[i].value / 100
                window.widgets[id].computed_size[i] = window.widgets[parent].computed_size[i] * percent
            }
        }
    }

    for i in 0..<len(window.widgets[id].children) {
        child_id := window.widgets[id].children[i]
        calc_parent_dependent(window, child_id, level + 1)
    }
}

calc_child_dependent :: proc(window: ^Window, id: uint, level: uint) {
    for i in 0..<len(window.widgets[id].children) {
        child_id := window.widgets[id].children[i]
        calc_child_dependent(window, child_id, level + 1)

        for j in 0..<2 {
            #partial switch window.widgets[id].size[j].kind {
                case .ChildrenSum: {
                    window.widgets[id].computed_size[j] += window.widgets[child_id].computed_size[j]
                }
                case .ChildrenMax: {
                    if window.widgets[id].computed_size[j] < window.widgets[child_id].computed_size[j] {
                        window.widgets[id].computed_size[j] = window.widgets[child_id].computed_size[j]
                    }
                }
            }
        }
    }
}

calc_violations :: proc(window: ^Window, id: uint, level: uint) {
    for i in 0..<len(window.widgets[id].children) {
        child_id := window.widgets[id].children[i]
        calc_violations(window, child_id, level + 1)
    }

    for j in 0..<2 {
        total: f32 = 0
        for i in 0..<len(window.widgets[id].children) {
            child_id := window.widgets[id].children[i]
            total += window.widgets[child_id].computed_size[j]
        }
        if total > window.widgets[id].computed_size[j] {
            difference := total - window.widgets[id].computed_size[j]
            available: f32 = 0
            for i in 0..<len(window.widgets[id].children) {
                child_id := window.widgets[id].children[i]
                available += window.widgets[child_id].computed_size[j] * (1 - window.widgets[child_id].size[j].strictness)
            }

            shrink_multiplier := difference / available
            if shrink_multiplier > 1 {
                for i in 0..<len(window.widgets[id].children) {
                    // child_id := window.widgets[id].children[i]
                    // TODO figure this out
                }
            } else {
                for i in 0..<len(window.widgets[id].children) {
                    child_id := window.widgets[id].children[i]
                    available := window.widgets[child_id].computed_size[j] * (1 - window.widgets[child_id].size[j].strictness)
                    window.widgets[child_id].computed_size[j] -= available * shrink_multiplier
                }
            }
        }
    }
}

calc_positions :: proc(window: ^Window, id: uint, level: uint, pos: [2]f32) {
    child_pos := pos
    for i in 0..<len(window.widgets[id].children) {
        child_id := window.widgets[id].children[i]
        calc_positions(window, child_id, level + 1, child_pos)
        switch window.widgets[id].layout {
            case .Floating: {
                child_pos = window.widgets[child_id].requested_pos
            }
            case .Horizontal: {
                child_pos.x += window.widgets[child_id].computed_size[0]
            }
            case .Vertical: {
                child_pos.y += window.widgets[child_id].computed_size[1]
            }
            case .ToolRow: {
                child_pos.x += window.widgets[child_id].computed_size[0]
            }
            case .ToolColumn: {
                child_pos.y += window.widgets[child_id].computed_size[1]
            }
        }
    }

    parent := window.widgets[id].parent
    window.widgets[id].rect = {
        window.rect.x + pos.x,
        window.rect.y + pos.y,
        window.widgets[id].computed_size[0],
        window.widgets[id].computed_size[1],
    }
}

draw_node :: proc(window: ^Window, id: uint, level: uint, style: ^StyleInfo) {
    // fmt.printf("%v draw %v: %v\n", level, window.widgets[id].name, window.widgets[id].rect);

    style.color_background = style.temp_colors[style.temp_color_idx]
    style.temp_color_idx = (style.temp_color_idx + 1) % 100

    color: [4]f32
    if window.widgets[id].hovered {
        color = {0.5, 0.5, 0.5, 1}
    } else {
        color = style.color_background
    }

    flags := window.widgets[id].flags
    if .DrawBorder in flags {
        app.draw_rect(window.widgets[id].rect, style.color_border)
        inside_rect: Rect = {
            window.widgets[id].rect.x + style.border_size,
            window.widgets[id].rect.y + style.border_size,
            window.widgets[id].rect.w - style.border_size * 2,
            window.widgets[id].rect.h - style.border_size * 2,
        }
        app.draw_rect(inside_rect, color)
    } else {
        app.draw_rect(window.widgets[id].rect, color)
    }

    if .DrawText in flags {
        app.draw_text(window.widgets[id].name, window.widgets[id].rect.x + style.padding, window.widgets[id].rect.y + style.padding, style.font_size, style.color_text)
    }

    for i in 0..<len(window.widgets[id].children) {
        child_id := window.widgets[id].children[i]
        draw_node(window, child_id, level + 1, style)
    }
}

calc_input :: proc(window: ^Window, id: uint, level: uint, mouse_intercepted: bool) {

    window.widgets[id].hovered = false

    for i in 0..<len(window.widgets[id].children) {
        child_id := window.widgets[id].children[i]
        calc_input(window, child_id, level + 1, mouse_intercepted)
    }

    mouse_pos := app.mouse_pos()

    if !(window.mouse_intercepted || mouse_intercepted) && app.rect_contains(window.widgets[id].rect, mouse_pos) {
        window.widgets[id].hovered = true
        window.mouse_intercepted = true
    }
}

check_widget :: proc(window: ^Window, widget: Widget) -> (uint, Interaction) {
    widget := widget
    interaction: Interaction

    has_id := false
    id: uint
    for widget_id in window.widgets[window.current_id].children {
        if window.widgets[widget_id].name == widget.name {
            has_id = true
            id = widget_id
        }
    }

    if has_id {
        mouse_pos := app.mouse_pos()

        if app.rect_contains(window.widgets[id].rect, mouse_pos) {
            interaction.hovered = true
        }

        if app.rect_contains(window.widgets[id].rect, mouse_pos) && app.mouse_pressed(.Left) {
            interaction.clicked = true
            if .Movable in window.widgets[id].flags {
                window.widgets[id].dragging = true
            }
        }

        if !app.mouse_down(.Left) {
            window.widgets[id].dragging = false
        }
    } else {
        widget.id = len(window.widgets)
        widget.parent = window.current_id
        append(&window.widgets[window.current_id].children, widget.id)
        id = widget.id
        append(&window.widgets, widget)
    }

    return id, interaction
}

// ============================================================


@(private)
state: Ui

init :: proc() {
    randstate: rand.Rand
    rand.init(&randstate, 1000)

    style: StyleInfo
    style.font_size = 20
    style.border_size = 2
    style.padding = 5
    style.color_background = {0.5, 0.5, 0.5, 1}
    style.color_border = {0, 0.5, 0, 1}
    style.color_text = {1, 1, 1, 1}
    style.temp_colors = make([dynamic][4]f32)

    for _ in 0..<100 {
        append(&style.temp_colors, [4]f32{
            rand.float32(&randstate),
            rand.float32(&randstate),
            rand.float32(&randstate),
            1,
        })
    }

    window: Window
    window.name = "FIRST_ROOT_WINDOW"

    widget: Widget
    widget.name = "FIRST_ROOT_WIDGET"
    widget.size = [2]Size{
        {.PercentOfParent, 100, 0},
        {.PercentOfParent, 100, 0},
    }
    widget.layout = .Floating

    state.next_floating_window_pos = {20, 40}
    state.style = style
    append(&state.windows, window)
    append(&state.windows[0].widgets, widget)
}

push_layout :: proc(name: string, layout: Layout) -> Interaction {
    w := state.current_id;
    size: [2]Size
    switch layout {
        case .Floating: size = {
            {.ChildrenSum, 0, 0},
            {.ChildrenSum, 0, 0},
        }
        case .Horizontal: size = {
            {.PercentOfParent, 100, 1},
            {.PercentOfParent, 100, 0},
        }
        case .Vertical: size = {
            {.PercentOfParent, 100, 0},
            {.PercentOfParent, 100, 1},
        }
        case .ToolRow: size = {
            {.PercentOfParent, 100, 1},
            {.ChildrenMax, 0, 1},
        }
        case .ToolColumn: size = {
            {.ChildrenMax, 0, 1},
            {.PercentOfParent, 100, 1},
        }
    }
    flags: WidgetFlags
    if layout == .Floating {
        flags += {.Movable}
    }
    widget: Widget
    widget.name = name
    widget.size = size
    widget.layout = layout
    widget.flags = flags
    new_id, interaction := check_widget(&state.windows[w], widget)
    state.windows[w].current_id = new_id;
    return interaction;
}

pop_layout :: proc() {
    w := state.current_id
    state.windows[w].current_id = state.windows[w].widgets[state.windows[w].current_id].parent
}

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
    widget.flags = {.Clickable, .DrawBorder, .DrawText}

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

check_window :: proc(window: Window) {
    rect := window.rect

    for w in 0..<len(state.windows) {
        if state.windows[w].name == window.name {
            state.current_id = uint(w)
            return
        }
    }

    state.current_id = len(state.windows)
    append(&state.windows, window)

    widget: Widget
    widget.name = "ANOTHER_ROOT_WIDGET"
    widget.size = {
        {.Pixels, window.rect.w, 1},
        {.Pixels, window.rect.h, 1},
    }
    widget.layout = .Floating
    append(&state.windows[state.current_id].widgets, widget)
}

push_window :: proc(name: string, rect: Rect) {
    window: Window
    window.name = name
    window.rect = rect
    check_window(window)
}

update :: proc() {
    state.current_id = 0
    state.mouse_intercepted = false
    state.style.temp_color_idx = 0

    window_size := app.window_size()
    state.windows[0].rect = {0, 0, window_size.x, window_size.y}

    for w in 0..<len(state.windows) {
        rect := state.windows[w].rect
        state.windows[w].widgets[0].size = {
            {.Pixels, rect.w, 1},
            {.Pixels, rect.h, 1},
        }
        state.windows[w].mouse_intercepted = state.mouse_intercepted
        state.windows[w].current_id = 0

        for i in 0..<len(state.windows[w].widgets) {
            for j in 0..<2 {
                #partial switch state.windows[w].widgets[i].size[j].kind {
                    case .Pixels: {
                        state.windows[w].widgets[i].computed_size[j] = state.windows[w].widgets[i].size[j].value
                    }
                    case .TextContent: {
                        text_rect := app.get_text_rect(state.windows[w].widgets[i].name, 0, 0, state.style.font_size)
                        // fmt.printf("the text rect is %v\n", text_rect)
                        text_size: [2]f32 = {
                            text_rect.w + (state.style.padding + state.style.border_size) * 2,
                            text_rect.h + (state.style.padding + state.style.border_size) * 2,
                        }
                        state.windows[w].widgets[i].computed_size[j] = text_size[j]
                    }
                }
            }
        }

        calc_parent_dependent(&state.windows[w], 0, 0)
        calc_child_dependent(&state.windows[w], 0, 0)
        calc_violations(&state.windows[w], 0, 0)
        calc_positions(&state.windows[w], 0, 0, {0, 0})
    }

    for w := len(state.windows) - 1; w >= 0; w -= 1 {
        calc_input(&state.windows[w], 0, 0, state.mouse_intercepted)
        if state.windows[w].mouse_intercepted {
            state.mouse_intercepted = true
        }
    }

    for w in 0..<len(state.windows) {
        for i in 0..<len(state.windows[w].widgets) {
            state.style.temp_color_idx = 0
            draw_node(&state.windows[w], 0, 0, &state.style)
        }
    }
}
