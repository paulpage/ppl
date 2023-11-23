use raylib::prelude::*;

fn main() {
    let (mut rl, thread) = raylib::init()
        .size(800, 600)
        .resizable()
        .title("Hello")
        .build();

    let mut y = 0.0;
    let mut text = String::from("hello");

    while !rl.window_should_close() {
        y += rl.get_mouse_wheel_move() * 30.0;

        while let Some(c) = rl.get_char_pressed() {
            text.push(c);
        }

        let mut d = rl.begin_drawing(&thread);
        d.clear_background(Color::BLUE);
        d.draw_text(text.as_str(), 12, 12 + y as i32, 20, Color::BLACK);
    }


}
