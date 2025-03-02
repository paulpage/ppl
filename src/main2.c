#include "platform.h"
#include "platform_sdl3.c"

int main(int argc, char **argv) {

    app_init();

    Font font = load_font("res/fonts/vera/Vera.ttf");
    Texture texture = load_texture("res/bird.png");

    while (!app_should_quit()) {

        app_clear((Color){0.0f, 0.0f, 1.0f, 1.0f});

        draw_rect(
            (Rect){20.0f, 20.0f, 40.0f, 50.0f},
            (Color){1.0f, 1.0f, 0.0f, 1.0f}
        );

        draw_texture(&texture, (Rect){0.0f, 0.0f, (f32)texture.w, (f32)texture.h}, (Rect){0.0f, 0.0f, (f32)texture.w, (f32)texture.h});

        draw_rect(
            (Rect){100.0f, 20.0f, 40.0f, 50.0f},
            (Color){1.0f, 1.0f, 0.0f, 1.0f}
        );

        draw_text(&font, "Hello", 0, 0);

    }

    app_quit();
    return 0;
}
