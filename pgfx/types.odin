package pgfx

rect_contains :: proc(rect: Rect, pos: [2]f32) -> bool {
    return (
        pos.x >= rect.x && pos.x <= rect.x + rect.w &&
        pos.y >= rect.y && pos.y <= rect.y + rect.h
    )
}
