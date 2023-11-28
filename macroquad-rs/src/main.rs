use macroquad::prelude as app;
use macroquad::input::KeyCode as Key;

fn get_text() -> Option<String> {
    let mut text = None;
    while let Some(c) = app::get_char_pressed() {
        if ((c as u32) < 57344 || (c as u32) > 63743) && !ctrl_down() && !alt_down() && !super_down() {
            text.get_or_insert(String::new()).push_str(&c.to_string());
        }
    }
    text
}

fn ctrl_down() -> bool {
    app::is_key_down(Key::LeftControl) || app::is_key_down(Key::RightControl)
}

fn alt_down() -> bool {
    app::is_key_down(Key::LeftAlt) || app::is_key_down(Key::RightAlt)
}

fn shift_down() -> bool {
    app::is_key_down(Key::LeftShift) || app::is_key_down(Key::RightShift)
}

fn super_down() -> bool {
    app::is_key_down(Key::LeftSuper) || app::is_key_down(Key::RightSuper)
}

fn window_conf() -> Conf {
    Conf {
        platform: miniquad::conf::Platform {
            linux_backend: miniquad::conf::LinuxBackend::WaylandOnly,
            ..Default::default()
        },
        ..Default::default()
    }
}

#[macroquad::main("hello")]
async fn main() {

    let mut pos_y = 0.0;

    let mut lines = Vec::new();
    lines.push(String::from("Hello"));
    let mut line_idx = 0;

    let mut scroll_string = String::new();

    loop {
        app::clear_background(app::RED);

        app::draw_line(40.0, 40.0, 100.0, 200.0, 15.0, app::BLUE);
        app::draw_rectangle(app::screen_width() / 2.0 - 60.0, 100.0, 120.0, 60.0, app::GREEN);
        app::draw_circle(app::screen_width() - 30.0, app::screen_height() - 30.0, 15.0, app::YELLOW);

        let (_wheel_x, wheel_y) = app::mouse_wheel();
        let wheel_y = wheel_y / 120.0;
        // pos_y += wheel_y * 30.0;
        pos_y += wheel_y * 30.0;

        if wheel_y != 0.0 {
            scroll_string = format!("scroll: {}", wheel_y);
        }

        if let Some(s) = get_text() {
            lines[line_idx].push_str(&s);
        }
        if app::is_key_pressed(Key::Enter) {
            lines.push(String::new());
            line_idx += 1;
        }

        app::draw_text(&scroll_string, 20.0, 20.0, 30.0, app::DARKGRAY);
        for (i, line) in lines.iter().enumerate() {
            app::draw_text(line.as_str(), 20.0, 20.0 + pos_y + 35.0 * (i + 1) as f32, 30.0, app::DARKGRAY);
        }

        app::next_frame().await
    }
}
