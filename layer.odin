package main

import "core:strings"
import "core:fmt"
import "core:container/queue"
import stbi "vendor:stb/image"

Rect :: struct {
    x, y, w, h: i32,
}
Color :: distinct [4]u8
Point :: distinct [2]i32

rect_union :: proc(a: Rect, b: Rect) -> Rect {
    if a.w == 0 || a.h == 0 {
        return b
    }
    if b.w == 0 || b.h == 0 {
        return a
    }
    return {
        min(a.x, b.x),
        min(a.y, b.y),
        max(a.x + a.w, b.x + b.w) - min(a.x, b.x),
        max(a.y + a.h, b.y + b.h) - min(a.y, b.y),
    }
}

rect_has_intersection :: proc(a: Rect, b: Rect) -> bool {
    return a.x <= b.x + b.w && a.x + a.w >= b.x && a.y <= b.y + b.h && a.y + a.h >= b.y
}

rect_intersection :: proc(a: Rect, b: Rect) -> Rect {
    if !rect_has_intersection(a, b) {
        return {0, 0, 0, 0}
    }
    return {
        max(a.x, b.x),
        max(a.y, b.y),
        min(a.x + a.w, b.x + b.w) - max(a.x, b.x),
        min(a.y + a.h, b.y + b.h) - max(a.y, b.y),
    }
}

rect_contains :: proc(r: Rect, p: Point) -> bool {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h
}

Layer :: struct {
    rect: Rect,
    data: [dynamic]Color,
    z_index: i32,
    dirty_rect: Rect,
}

Image :: struct {
    rect: Rect,
    layers: [dynamic]Layer,
    blended: Layer,
}

ImageHistory :: struct {
    snapshots: [dynamic]Image,
    idx: i32,
}

// ------------------------------------------------------------

make_layer :: proc(rect: Rect) -> Layer {
    layer := Layer{
        rect = rect,
        data = make([dynamic]Color, rect.w * rect.h),
        z_index = 0,
        dirty_rect = {0, 0, 0, 0},
    }

    for i in 0..<rect.w * rect.h {
        layer.data[i] = {0, 0, 0, 0}
    }

    return layer
}

make_layer_from_path :: proc(path: string, p: Point) -> Layer {
    w, h, channels: i32
    data := stbi.load(strings.clone_to_cstring(path), &w, &h, &channels, 4)
    fmt.printf("loaded image, w=%v, h=%v, channels=%v\n", w, h, channels)

    layer := Layer{
        rect = {p.x, p.y, w, h},
        data = make([dynamic]Color, w * h), 
        z_index = 0,
        dirty_rect = {0, 0, 0, 0},
    }

    for i in 0..<w*h {
        layer.data[i].r = data[i * 4 + 0]
        layer.data[i].g = data[i * 4 + 1]
        layer.data[i].b = data[i * 4 + 2]
        layer.data[i].a = data[i * 4 + 3]
    }

    return layer
}

layer_add_dirty_rect :: proc(layer: ^Layer, rect: Rect) {
    layer.dirty_rect = rect_union(layer.dirty_rect, rect)
}

layer_clear_dirty_rect :: proc(layer: ^Layer) {
    layer.dirty_rect = {0, 0, 0, 0}
}

layer_contains_point :: proc(layer: ^Layer, p: Point) -> bool {
    rect := Rect{0, 0, layer.rect.w, layer.rect.h}
    return rect_contains(rect, p)
}

layer_draw_pixel_unchecked :: proc(layer: ^Layer, p: Point, color: Color) {
    if color != {255, 255, 255, 255} {
        // fmt.printf("actually drawing pixel: %v\n", color)
    }
    layer.data[int(p.y * layer.rect.w + p.x)] = color
}

layer_draw_pixel :: proc(layer: ^Layer, p: Point, color: Color) {
    if layer_contains_point(layer, p) {
        layer_draw_pixel_unchecked(layer, p, color)
    }
}

layer_get_pixel_unchecked :: proc(layer: ^Layer, p: Point) -> Color {
    return layer.data[int(p.y * layer.rect.w + p.x)]
}

layer_get_pixel :: proc(layer: ^Layer, p: Point) -> (color: Color, is_present: bool) {
    if !layer_contains_point(layer, p) {
        return {0, 0, 0, 0}, false
    }
    return layer_get_pixel_unchecked(layer, p), true
}

layer_draw_line :: proc(layer: ^Layer, p1, p2: Point, color: Color) {
    layer_draw_pixel(layer, p1, color)
    layer_draw_pixel(layer, p2, color)
    w := abs(p2.x - p1.x)
    h := abs(p2.y - p1.y)
    step := max(w, h)
    if step != 0 {
        dx := (f64(p2.x) - f64(p1.x)) / f64(step)
        dy := (f64(p2.y) - f64(p1.y)) / f64(step)
        for i in 0..<step {
            layer_draw_pixel(layer, {i32(f64(p1.x) + dx * f64(i)), i32(f64(p1.y) + dy * f64(i))}, color)
        }
    }
    layer_add_dirty_rect(layer, Rect{
        min(p1.x, p2.x) - 1,
        min(p1.y, p2.y) - 1,
        w + 2,
        h + 2,
    })
}

layer_fill :: proc(layer: ^Layer, target: Point, color: Color) {
    target_color, ok := layer_get_pixel(layer, target)
    if !ok || target_color == color {
        return
    }
    layer_draw_pixel_unchecked(layer, target, color)
    q: queue.Queue(Point)
    queue.push_back(&q, target)
    p, p2: Point
    for queue.len(q) > 0 {
        p, ok = queue.pop_front_safe(&q)
        if !ok {
            break
        }

        old_color: Color

        points := [4]Point{
            {p.x - 1, p.y},
            {p.x + 1, p.y},
            {p.x, p.y - 1},
            {p.x, p.y + 1},
        }
        for p2 in points {
            old_color, ok = layer_get_pixel(layer, p2)
            if ok {
                if old_color == target_color {
                    layer_draw_pixel_unchecked(layer, p2, color)
                    queue.push_back(&q, p2)
                }
            }
        }
    }
    layer_add_dirty_rect(layer, layer.rect)
}

layer_blend :: proc(layer: ^Layer, other: ^Layer, clip_rect: Rect) -> bool {
    target_rect := rect_intersection(
        rect_intersection(
            rect_intersection(layer.rect, other.rect), 
            rect_union(layer.dirty_rect, other.dirty_rect),
        ),
        clip_rect,
    )

    if target_rect.w == 0 || target_rect.h == 0 {
        return false
    }

    for y in target_rect.y..<target_rect.y + target_rect.h {
        for x in target_rect.x..<target_rect.x + target_rect.w {
            if !(rect_contains(layer.rect, {x, y}) && rect_contains(other.rect, {x - other.rect.x, y - other.rect.y})) {
                // TODO what am I checking here?
            }
            base_color := layer_get_pixel_unchecked(layer, {x, y})
            other_color := layer_get_pixel_unchecked(other, {x - other.rect.x, y - other.rect.y})

            if other_color.a == 0 {
                continue
            }
            if other_color.a == 255 {
                layer_draw_pixel(layer, {x, y}, other_color)
                continue
            }

            a1 := f64(other_color.a) / 255
            a2 := f64(base_color.a) / 255
            factor := a2 * (1 - a1)

            new_color := Color{
                u8(f64(base_color.r) * a1 + f64(other_color.r) * factor * 255 / (a1 + factor)),
                u8(f64(base_color.g) * a1 + f64(other_color.g) * factor * 255 / (a1 + factor)),
                u8(f64(base_color.b) * a1 + f64(other_color.b) * factor * 255 / (a1 + factor)),
                u8(f64(base_color.a) * a1 + f64(other_color.a) * factor * 255 / (a1 + factor)),
            }
            layer_draw_pixel(layer, {x, y}, new_color)
        }
    }

    return true
}

// ------------------------------------------------------------

make_image :: proc(w, h: i32) -> Image {
    rect := Rect{0, 0, w, h}
    image := Image{
        rect = rect,
        layers = make([dynamic]Layer),
        blended = make_layer(rect),
    }
    append(&image.layers, make_layer(rect))
    return image
}

make_image_from_path :: proc(path: string) -> (image: Image, ok: bool) {
    layer := make_layer_from_path(path, {0, 0})
    image = Image{
        rect = layer.rect,
        layers = make([dynamic]Layer),
        blended = make_layer(layer.rect),
    }
    append(&image.layers, layer)
    return image, true
}

image_get_dirty_rect :: proc(image: ^Image) -> Rect {
    dirty_rect := Rect{0, 0, 0, 0}
    for layer in image.layers {
        dirty_rect = rect_union(dirty_rect, layer.dirty_rect)
    }
    return dirty_rect
}

image_clear_dirty_rect :: proc(image: ^Image) {
    for _, i in image.layers {
        layer_clear_dirty_rect(&image.layers[i])
    }
}

image_blend :: proc(image: ^Image, clip_rect: Rect) {
    for _, i in image.blended.data {
        image.blended.data[i] = {0, 0, 0, 0}
    }
    for _, i in image.layers {
        layer_blend(&image.blended, &image.layers[i], clip_rect)
    }
}

image_get_raw_data :: proc(image: ^Image) -> [dynamic]Color {
    image_blend(image, image.rect)
    return image.blended.data
}

// image_get_partial_data :: proc(image: ^Image, rect: Rect) -> [dynamic]u8 {
//     image_blend(image, rect)
//     data := make([dynamic]u8, rect.w * rect.h * 4)
//     for y in rect.y..<rect.y+rect.h {
//         for x in rect.x..<rect.x+rect.w {
//             si := (y - rect.y) * rect.w + (x-rect.x)
//             di := y * blended.rect.w + x
//             color := blended.data[di]
//             data[si * 4 + 0] = color.r
//             data[si * 4 + 1] = color.g
//             data[si * 4 + 2] = color.b
//             data[si * 4 + 3] = color.a
//         }
//     }
//     return data
// }

image_save :: proc(image: ^Image, path: string) {
    image_blend(image, image.rect)
    stbi.write_png(
        strings.clone_to_cstring(path),
        image.rect.w, image.rect.h, 4,
        raw_data(image.blended.data),
        4 * image.rect.w
    )
}
