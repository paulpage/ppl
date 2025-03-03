#include "platform.h"
#include "platform_sdl3.c"
#include "sound_sdl3.c"

int main(int argc, char **argv) {

    app_init();

    Sound sound = load_sound("res/tweet.ogg");
    Sound music = load_sound("res/song.ogg");

    Font font = load_font("res/fonts/vera/Vera.ttf");
    Texture texture = load_texture("res/bird.png");

    play_sound(&sound);

    while (!app_should_quit()) {
        play_music(&music);

        app_clear((Color){0.0f, 0.0f, 1.0f, 1.0f});

        if (is_mouse_down(BUTTON_LEFT)) {
            draw_rect(
                (Rect){20.0f, 20.0f, 40.0f, 50.0f},
                (Color){1.0f, 1.0f, 0.0f, 1.0f}
            );
        }

        draw_texture(&texture, (Rect){0.0f, 0.0f, (f32)texture.w, (f32)texture.h}, (Rect){0.0f, 0.0f, (f32)texture.w, (f32)texture.h});

        draw_rect(
            (Rect){100.0f, 20.0f, 40.0f, 50.0f},
            (Color){1.0f, 1.0f, 0.0f, 1.0f}
        );

        draw_text(&font, "Hello", 0, 0);

        if (is_key_pressed(KEY_A)) {
            printf("a");
        }

        if (is_key_pressed(KEY_Q)) {
            app_quit();
        }

    }

    return 0;
}
