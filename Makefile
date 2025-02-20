CFLAGS += $(shell pkg-config sdl3 --cflags)
LDFLAGS += -lm $(shell pkg-config sdl3 --libs)
SHADERS := $(wildcard shaders/*.vert shaders/*.frag)
SPV_FILES := $(SHADERS:%=%.spv)

default: main

render: render.c
	${CC} -o $@ $< ${CFLAGS} ${LDFLAGS}

gl: gl.c
	${CC} -o $@ $< ${CFLAGS} -lGL ${LDFLAGS}

gpu: gpu.c
	${CC} -o $@ $< ${CFLAGS} ${LDFLAGS}

.PHONY: shaders
shaders: $(SPV_FILES)

%.vert.spv: %.vert
	glslangValidator -V $< -o $@

%.frag.spv: %.frag
	glslangValidator -V $< -o $@

run: render
	./render

clean:
	rm -f main

