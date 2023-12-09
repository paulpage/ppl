package main

import "core:fmt"
import "core:time"
import "core:strings"

import app "pgfx"

main :: proc() {

    app.init("App")

    texture := app.load_texture("../res/bird.png")

    pos: [2]f32
    scroll: f32

    data := #load("pgfx/drawing.odin")

    for app.update() {
        start := time.tick_now()

        if app.key_pressed(.A) {
            app.play_sound("../res/tweet.ogg")
        }

        pos = app.mouse_pos()
        scroll += app.mouse_wheel().y

        app.clear_background({0, 0.5, 0, 1})
        app.draw_texture_ex(texture, {0, 0, texture.w, texture.h}, {pos.x, pos.y + scroll * 40, texture.w, texture.h}, 0, 0, {1, 1, 1, 1}, false)
        app.draw_texture_ex(app.state.font_texture, {0, 0, app.state.font_texture.w, app.state.font_texture.h}, {pos.x, pos.y /*+ scroll * 40*/, app.state.font_texture.w, app.state.font_texture.h}, 0, 0, {1, 1, 1, 1}, false)
        // app.draw_rect_ex({pos.x, pos.y + scroll * 40, texture.w / 2, texture.h / 4}, 0, 0, {0, 0.5, 0.5, 1})

        i := 0
        it := string(data)
        for line in strings.split_lines_iterator(&it) {
            i += 1
            app.draw_text(line, 5, 5 + 20 * f32(i), 20 + scroll, {1, 1, 0, 1})
        }

        text := app.text_entered()
        if text != "" {
            fmt.println(text)
        }

        app.present()

        // fmt.printf("=============== frame time: %v\n", time.tick_since(start))
    }
    app.quit()
}
