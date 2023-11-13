#include <stdio.h>

#include "ppl.h"

static void init(void) {
    ppl_init();
}

void frame(void) {

    ppl_draw();
}

void cleanup(void) {
    ppl_quit();
}

void event(const sapp_event *e) {
    ppl_handle_event(e);
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 800,
        .height = 600,
        .window_title = "App",
    };
}
