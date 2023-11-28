#!/bin/sh
curl -LO https://wayland.freedesktop.org/wayland.png
convert wayland.png wayland.ppm
xxd -s +15 -i wayland.ppm > wayland-logo.h
sed -i 's/wayland_ppm/wayland_logo/g' wayland-logo.h
