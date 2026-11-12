#include "keyboard_keys.hpp"

KeyEvent decodeKeyboardKey(uint8_t modifiers, uint8_t usage) {
    KeyEvent event;
    event.control = (modifiers & 0x11) != 0;
    event.alt = (modifiers & 0xcc) != 0;  // Alt/GUI combinations are not application shortcuts.
    bool shift = (modifiers & 0x22) != 0;
    if (usage >= 4 && usage <= 29) {
        event.key = Key::Character;
        event.character = (shift ? 'A' : 'a') + usage - 4;
    } else if (usage >= 30 && usage <= 39) {
        event.key = Key::Character;
        event.character = (shift ? "!@#$%^&*()" : "1234567890")[usage - 30];
    } else if (usage >= 45 && usage <= 56 && usage != 50) {
        // HID 0x32 is the non-US key; the TAB5 uses the US key layout.
        constexpr char plain[] = "-=[]\\#;'`,./";
        constexpr char shifted[] = "_+{}|~:\"~<>?";
        event.key = Key::Character;
        event.character = (shift ? shifted : plain)[usage - 45];
    } else if (usage >= 89 && usage <= 97) {
        event.key = Key::Character;
        event.character = '1' + usage - 89;
    } else {
        switch (usage) {
            case 40: event.key = Key::Enter; break;
            case 41: event.key = Key::Escape; break;
            case 42: event.key = Key::Backspace; break;
            case 43: event.key = Key::Tab; break;
            case 44: event.key = Key::Character; event.character = ' '; break;
            case 75: event.key = Key::PageUp; break;
            case 76: event.key = Key::Delete; break;
            case 78: event.key = Key::PageDown; break;
            case 79: event.key = Key::Right; break;
            case 80: event.key = Key::Left; break;
            case 81: event.key = Key::Down; break;
            case 82: event.key = Key::Up; break;
            case 84: event.key = Key::Character; event.character = '/'; break;
            case 85: event.key = Key::Character; event.character = '*'; break;
            case 86: event.key = Key::Character; event.character = '-'; break;
            case 87: event.key = Key::Character; event.character = '+'; break;
            case 88: event.key = Key::Enter; break;
            case 98: event.key = Key::Character; event.character = '0'; break;
            case 99: event.key = Key::Character; event.character = '.'; break;
            default: break;  // Ignore releases, rollover errors and modifier-only events.
        }
    }
    return event;
}
