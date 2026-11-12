#include "usb_keyboard_report.hpp"
#include <algorithm>

size_t UsbKeyboardReport::parse(const uint8_t* report, size_t length, std::array<KeyEvent, 6>& pressed) {
    if (!report || length != 8) return 0;
    // Rollover/error usages do not represent releases. Retain the last good
    // state so a transient error cannot turn a held Enter into another send.
    for (size_t i = 2; i < 8; ++i) if (report[i] >= 1 && report[i] <= 3) return 0;
    if (std::find(report + 2, report + 8, 57) != report + 8 &&
        std::find(previous_.begin(), previous_.end(), 57) == previous_.end()) capsLock_ = !capsLock_;
    size_t count = 0;
    for (size_t i = 2; i < 8; ++i) {
        uint8_t usage = report[i];
        if (!usage || std::find(previous_.begin(), previous_.end(), usage) != previous_.end() ||
            std::find(report + 2, report + i, usage) != report + i) continue;
        if (usage == 57) continue;
        uint8_t modifiers = report[0];
        if (capsLock_ && usage >= 4 && usage <= 29) {
            bool shifted = (modifiers & 0x22) != 0;
            modifiers &= ~0x22;
            if (!shifted) modifiers |= 0x02;
        }
        auto event = decodeKeyboardKey(modifiers, usage);
        if (event.key != Key::None) pressed[count++] = event;
    }
    std::copy(report + 2, report + 8, previous_.begin());
    return count;
}
