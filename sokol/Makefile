main:
	mkdir -p target
	mkdir -p target/shaders
	sokol-shdc -i src/shaders/shape.glsl -o target/shaders/shape.glsl.h --slang=glsl330
	${CC} src/main.c src/ppl.c -g -Isrc -Ilib -Itarget/shaders -lX11 -lXi -lXcursor -lGL -ldl -lpthread -lm -o target/main
run: main
	target/main
