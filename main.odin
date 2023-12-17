package main

import "core:fmt"
import "core:time"
import "core:strings"
import "core:math"
import "core:math/rand"

import app "pgfx"
import "ui"

Tool :: enum {
    Pencil,
    Paintbrush,
    ColorPicker,
    Fill,
    SprayCan,
}

tool_names := map[Tool]string{
    .Pencil = "Pencil",
    .Paintbrush = "Paintbrush",
    .ColorPicker = "Color Picker",
    .Fill = "Fill",
    .SprayCan = "Spray Can",
}

State :: struct {
    image: Image,
    layer: ^Layer,
    texture: app.Texture,

    canvas_pos: [2]f32,
    canvas_scale: f32,
    canvas_offset: [2]f32,
    canvas_offset_baseline: [2]f32,

    tool: Tool,
    color: Color,

    drawing: bool,

    window_size: [2]f32,
    old_mouse_pos: [2]f32,
}

mouse_to_canvas :: proc(mouse_pos: [2]f32, canvas_pos: [2]f32, canvas_scale: f32) -> Point {
    return Point{
        i32((mouse_pos.x - canvas_pos.x) / canvas_scale),
        i32((mouse_pos.y - canvas_pos.y) / canvas_scale),
    }
}

update_canvas_pos :: proc(state: ^State) {
    state.canvas_pos += state.canvas_offset
}

main :: proc() {
    randstate: rand.Rand
    rand.init(&randstate, u64(time.tick_now()._nsec))

    app.init("App")

    // texture := app.load_texture("res/bird.png")

    pos: [2]f32
    scroll: f32

    ui.init()

    state := State{
        image = make_image(800, 600),
        canvas_pos = {100, 100},
        canvas_scale = 2,
        canvas_offset = {0, 0},
        canvas_offset_baseline = {0, 0},
        tool = .Pencil,
        color = {255, 255, 0, 255},
        drawing = false,
        window_size = app.window_size(),
        old_mouse_pos = app.mouse_pos(),
    }

    state.layer = &state.image.layers[0]
    for i in 0..<state.image.rect.w * state.image.rect.h {
        state.image.layers[0].data[i] = Color{255, 255, 255, 255}
    }
    state.texture = app.load_texture_mem(state.image.rect.w, state.image.rect.h, raw_data(image_get_raw_data(&state.image)[:]))
    state.image.layers[0].dirty_rect = state.image.layers[0].rect
    state.layer = &state.image.layers[0]

    for app.update() {

        mouse_intercepted := false

        // Set active tool
        if app.key_pressed(.P) {
            state.tool = .Pencil
            fmt.println(tool_names[state.tool])
        }
        if app.key_pressed(.B) {
            state.tool = .Paintbrush
            fmt.println(tool_names[state.tool])
        }
        if app.key_pressed(.C) {
            state.tool = .ColorPicker
            fmt.println(tool_names[state.tool])
        }
        if app.key_pressed(.F) {
            state.tool = .Fill
            fmt.println(tool_names[state.tool])
        }
        if app.key_pressed(.S) {
            state.tool = .SprayCan
            fmt.println(tool_names[state.tool])
        }

        if app.key_pressed(.W) {
            image_save(&state.image, "test.png")
        }

        if app.mouse_pressed(.Middle) {
            mouse_pos := app.mouse_pos()
            state.canvas_offset_baseline = mouse_pos
        }
        if app.mouse_down(.Middle) {
            mouse_pos := app.mouse_pos()
            state.canvas_offset = mouse_pos - state.canvas_offset_baseline
            update_canvas_pos(&state)
            state.canvas_offset = {0, 0}
            state.canvas_offset_baseline = mouse_pos
        }

        scroll := app.mouse_wheel()
        if scroll.y != 0 {
            state.canvas_scale *= (10 + scroll.y) / 10
            update_canvas_pos(&state)
        }


        // Update Image
        if (app.mouse_down(.Left) && !mouse_intercepted)|| state.drawing {

            mouse_pos := app.mouse_pos()
            pos1 := mouse_to_canvas({state.old_mouse_pos.x, state.old_mouse_pos.y}, state.canvas_pos, state.canvas_scale)
            pos2 := mouse_to_canvas({mouse_pos.x, mouse_pos.y}, state.canvas_pos, state.canvas_scale)

            switch state.tool {
                case .Pencil: {
                    layer_draw_line(state.layer, pos1, pos2, state.color)
                }
                case .Paintbrush: {
                    for dy in i32(-10)..=i32(10) {
                        for dx in i32(-10)..=i32(10) {
                            p1 := Point{pos1.x + dx, pos1.y + dy}
                            p2 := Point{pos2.x + dx, pos2.y + dy}
                            if math.sqrt(f64(dx*dx) + f64(dy*dy)) < 10 {
                                layer_draw_line(state.layer, p1, p2, state.color)
                            }
                        }
                    }
                }
                case .ColorPicker: {
                    color, ok := layer_get_pixel(state.layer, pos2)
                    if ok {
                        state.color = color
                        fmt.printf("selected color is %v\n", state.color)
                    }
                }
                case .Fill: {
                    layer_fill(state.layer, pos2, state.color)
                }
                case .SprayCan: {
                    for _ in 0..<100 {
                        dx := i32(rand.uint32(&randstate)) % 100 - 50
                        dy := i32(rand.uint32(&randstate)) % 100 - 50
                        if math.sqrt(f64(dx*dx) + f64(dy*dy)) < 50 {
                            layer_draw_pixel(state.layer, {pos2.x + dx, pos2.y + dy}, state.color)
                        }
                        layer_add_dirty_rect(state.layer, {pos2.x - 51, pos2.y - 51, 102, 102})
                    }
                }
            }
        }

        app.clear_background({0, 0.5, 0, 1})

        // TODO texture_update_part
        state.image.layers[0].dirty_rect = state.image.layers[0].rect
        app.texture_update(&state.texture, 0, 0, state.image.rect.w, state.image.rect.h, raw_data(image_get_raw_data(&state.image)[:]))

        app.draw_texture(state.texture,
            {0, 0, state.texture.w, state.texture.h},
            {state.canvas_pos.x, state.canvas_pos.y, state.texture.w * state.canvas_scale, state.texture.h * state.canvas_scale})

        // if app.key_pressed(.Q) || app.key_pressed(.RETURN) {
        //     break
        // }

        // ui.push_layout("Main Window", .Vertical)
        // ui.push_layout("Toolbar", .ToolRow)
        // if ui.button("Open").clicked {
        //     fmt.println("Open")
        // }
        // if ui.button("Save").clicked {
        //     fmt.println("Save")
        // }
        // ui.spacer("toolbar_spacer")
        // if ui.button("Close").clicked {
        //     fmt.println("Close")
        // }
        // ui.pop_layout()

        // ui.spacer("viewport_spacer")
        // ui.button("test")
        // ui.spacer("viewport_spacer2")

        // ui.push_layout("Status bar", .ToolRow)
        // if ui.button("Open2").clicked {
        //     fmt.println("Open2")
        // }
        // if ui.button("Save2").clicked {
        //     fmt.println("Save2")
        // }
        // ui.spacer("toolbar_spacer")
        // if ui.button("Close2").clicked {
        //     fmt.println("Close2")
        // }
        // ui.pop_layout()

        // ui.pop_layout()

        // ui.pop_layout()
        // ui.pop_layout()

        // ui.push_window("Tool Pane", {50, 50, 100, 300})
        // ui.push_layout("Tool columns", .ToolColumn)
        // ui.button("tool1")
        // ui.button("tool2")
        // ui.button("tool3")
        // ui.button("tool4")

        // ui.update()




        // if !has_updated || app::is_key_pressed(Key::Space) {
        //     has_updated = true
        // }



        // start := time.tick_now()

        // if app.key_pressed(.A) {
        //     app.play_sound("../res/tweet.ogg")
        // }

        // pos = app.mouse_pos()
        // scroll += app.mouse_wheel().y

        // app.clear_background({0, 0.5, 0, 1})

        // app.draw_rect({100, 100, 200, 200}, {0.5, 0.5, 1, 1})

        // app.draw_texture(texture,
        //     {0, 0, texture.w, texture.h},
        //     {pos.x, pos.y + scroll * 40, texture.w, texture.h})


        // i := 0
        // it := string(data)
        // for line in strings.split_lines_iterator(&it) {
        //     i += 1
        //     app.draw_text(line, 5, 5 + 20 * f32(i) + scroll * 40, 20, {1, 1, 0, 1})
        // }

        // text := app.text_entered()
        // if text != "" {
        //     fmt.println(text)
        // }


        state.old_mouse_pos = app.mouse_pos()

        app.present()

        // fmt.printf("=============== frame time: %v\n", time.tick_since(start))
    }
    app.quit()
}
