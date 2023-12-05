package main

import "core:fmt"

import app "pgfx"

main :: proc() {

    app.init("App")

    texture := app.load_texture("../res/bird.png")

    pos: [2]f32
    scroll: f32

    for app.update() {

        pos = app.mouse_pos()
        scroll += app.mouse_wheel().y

        app.clear_background({0, 0.5, 0, 1})
        app.draw_texture_ex(texture, {0, 0, texture.w, texture.h}, {pos.x, pos.y + scroll * 40, texture.w, texture.h}, 0, 0, {1, 1, 1, 1})
        app.draw_rect_ex({pos.x, pos.y + scroll * 40, texture.w / 2, texture.h / 4}, 0, 0, {0, 0.5, 0.5, 1})

        text := app.text_entered()
        if text != "" {
            fmt.println(text)
        }

        app.present()
    }
    app.quit()
}
