#pragma once
#include <cstdint>

enum class Key { None, Character, Enter, Escape, Backspace, Delete, Tab, Left, Right, Up, Down, PageUp, PageDown };
struct KeyEvent {
    Key key = Key::None;
    char character = 0;
    bool control = false, alt = false;
};
// Convert a keyboard HID usage/modifier pair to a US-layout application key.
KeyEvent decodeKeyboardKey(uint8_t modifiers, uint8_t usage);
