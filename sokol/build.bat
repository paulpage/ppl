@echo off

IF NOT EXIST target mkdir target
IF NOT EXIST target\shaders mkdir target\shaders
pushd target

:: sokol-shdc -i ..\src\shaders\shape.glsl -o ..\src\shaders\shape.glsl.h --slang=glsl330
sokol-shdc -i ..\src\shaders\shape.glsl -o ..\target\shaders\shape.glsl.h --slang=hlsl4

cl -nologo -Zi -I..\lib -I..\src -I..\src -I..\target\shaders ..\src\main.c

popd
