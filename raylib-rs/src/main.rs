use ::input::{Libinput, LibinputInterface};
use libc::{O_RDONLY, O_RDWR, O_WRONLY};
use std::fs::{File, OpenOptions};
use std::os::unix::{fs::OpenOptionsExt, io::OwnedFd};
use std::path::Path;

use raylib::prelude::*;


struct Interface;

impl LibinputInterface for Interface {
    fn open_restricted(&mut self, path: &Path, flags: i32) -> Result<OwnedFd, i32> {
        OpenOptions::new()
            .custom_flags(flags)
            .read((flags & O_RDONLY != 0) | (flags & O_RDWR != 0))
            .write((flags & O_WRONLY != 0) | (flags & O_RDWR != 0))
            .open(path)
            .map(|file| file.into())
            .map_err(|err| err.raw_os_error().unwrap())
    }
    fn close_restricted(&mut self, fd: OwnedFd) {
        drop(File::from(fd));
    }
}

fn main() {
    let (mut rl, thread) = raylib::init()
        .size(800, 600)
        .resizable()
        .title("Hello")
        .build();

    let mut input = Libinput::new_with_udev(Interface);
    input.udev_assign_seat("seat0").unwrap();

    let mut y = 0.0;
    let mut text = String::from("hello");

    while !rl.window_should_close() {
        y += rl.get_mouse_wheel_move() * 30.0;

        input.dispatch().unwrap();
        for event in &mut input {
            println!("Got event: {:?}", event);
        }

        while let Some(c) = rl.get_char_pressed() {
            text.push(c);
        }

        let mut d = rl.begin_drawing(&thread);
        d.clear_background(Color::BLUE);
        d.draw_text(text.as_str(), 12, 12 + y as i32, 20, Color::BLACK);
    }


}
