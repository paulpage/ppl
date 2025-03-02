#ifndef PLATFORM_H
#define PLATFORM_H

#include "handmade_math.h"
#include "types.h"

typedef struct AppConfig {
    char *title;
    int w, h;
} AppConfig;

typedef struct Rect {
    float x, y, w, h;
} Rect;

typedef Vec4 Color;

typedef struct Texture {
    void *handle; // SDL_GPUTexture*
    int w, h, d;
    int idx;
} Texture;

typedef struct Font {
    Texture texture;
    void *char_data; // stbtt_packedchar[96]
    float scale;
} Font;

typedef struct Sound Sound;


void app_init();
bool app_should_quit();
void app_quit();

void app_clear(Color color);

void draw_rect(Rect rect, Color color);
void draw_texture(Texture *texture, Rect src, Rect dst);
void draw_text(Font *font, const char *text, float x, float y);

Sound load_sound(char *filename);
void play_sound(Sound *sound);
void play_music(Sound *sound);
void stop_music();

#endif // PLATFORM_H
