package main

import "core:fmt"
import "core:time"
import "core:strings"

import app "pgfx"
import "ui"

main :: proc() {

    app.init("App")

    texture := app.load_texture("../res/bird.png")

    pos: [2]f32
    scroll: f32

    data := #load("pgfx/drawing.odin")

    ui.init()

    for app.update() {

        if app.key_pressed(.Q) || app.key_pressed(.RETURN) {
            break
        }

        ui.push_layout("Main Window", .Vertical)
        ui.push_layout("Toolbar", .ToolRow)
        if ui.button("Open").clicked {
            fmt.println("Open")
        }
        if ui.button("Save").clicked {
            fmt.println("Save")
        }
        ui.spacer("toolbar_spacer")
        if ui.button("Close").clicked {
            fmt.println("Close")
        }
        ui.pop_layout()

        ui.spacer("viewport_spacer")
        ui.button("test")
        ui.spacer("viewport_spacer2")

        ui.push_layout("Status bar", .ToolRow)
        if ui.button("Open2").clicked {
            fmt.println("Open2")
        }
        if ui.button("Save2").clicked {
            fmt.println("Save2")
        }
        ui.spacer("toolbar_spacer")
        if ui.button("Close2").clicked {
            fmt.println("Close2")
        }
        ui.pop_layout()

        ui.pop_layout()

        ui.pop_layout()
        ui.pop_layout()

        ui.push_window("Tool Pane", {50, 50, 100, 300})
        ui.push_layout("Tool columns", .ToolColumn)
        ui.button("tool1")
        ui.button("tool2")
        ui.button("tool3")
        ui.button("tool4")

        ui.update()
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


        app.present()

        // fmt.printf("=============== frame time: %v\n", time.tick_since(start))
    }
    app.quit()
}
