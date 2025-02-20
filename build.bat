@echo off

set FLAGS=/nologo /Zi
set LDFLAGS=/DEBUG:FULL /SUBSYSTEM:CONSOLE
set LIBS=SDL3.lib kernel32.lib user32.lib winmm.lib gdi32.lib opengl32.lib shell32.lib

set LIBDIR=C:\dev\ext\SDL\build
set BINDIR=C:\dev\lib\sdl3\bin
set INCDIR=/I C:\dev\lib\sdl3\include /I ..\lib\

set SRC=..\src\main.c

REM copy %LIBDIR%\SDL3.dll .

pushd bin
cl %FLAGS% %SRC% SDL3.lib %FLAGS% %INCDIR% /link %LDFLAGS% /LIBPATH:%LIBDIR% %LIBS%
popd

%BINDIR%\shadercross.exe shaders\2d.vert.hlsl -o shaders\2d.vert.spv
%BINDIR%\shadercross.exe shaders\2d.frag.hlsl -o shaders\2d.frag.spv
