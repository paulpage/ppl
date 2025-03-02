#include "stb_vorbis.h"


struct Sound {
    int len;
    int channels;
    int sample_rate;
    i16 *data;
    SDL_AudioStream *stream;
};


bool str_ends_with(char *str, char *suffix) {
    int len = 0;
    while (str[len] != '\0') len++;

    int suffix_len = 0;
    while (suffix[suffix_len] != '\0') suffix_len++;

    if (suffix_len > len) return false;

    for (int i = 1; i <= suffix_len; i++) {
        if (str[len - i] != suffix[suffix_len - i]) return false;
        printf("%c == %c\n", str[len - i], suffix[suffix_len - i]);
    }

    return true;
}

void sdl_sound_init() {
}

Sound load_sound(char *filename) {
    /* size_t len; */
    /* u8 *data = os_read_file(filename, &len); */
    Sound sound = {0};

    if (str_ends_with(filename, ".ogg")) {
        printf("It's an ogg\n");
        sound.len = stb_vorbis_decode_filename(filename, &sound.channels, &sound.sample_rate, &sound.data);
        printf("%d channels, sample rate %d, len %d\n", sound.channels, sound.sample_rate, sound.len);

        SDL_AudioSpec spec = {
            .format = SDL_AUDIO_S16,
            .channels = sound.channels,
            .freq = sound.sample_rate,
        };
        sound.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        SDL_PutAudioStreamData(sound.stream, sound.data, sound.len * sound.channels * sizeof(i16));
    }

    return sound;
}

void play_sound(Sound *sound) {
    SDL_ResumeAudioStreamDevice(sound->stream);
}

void play_music(Sound *music) {
    int len = music->len * music->channels * sizeof(i16);
    if (SDL_GetAudioStreamQueued(music->stream) < len) {
        SDL_PutAudioStreamData(music->stream, music->data, len);
    }
    SDL_ResumeAudioStreamDevice(music->stream);
}

void pause_music(Sound *music) {
    SDL_PauseAudioStreamDevice(music->stream);
}
