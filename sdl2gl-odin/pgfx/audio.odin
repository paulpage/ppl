package pgfx

import "core:strings"

import ma "vendor:miniaudio"

play_sound :: proc(filename: string) {
    ma.engine_play_sound(&state.audio_engine, strings.clone_to_cstring(filename), nil)
}
