#include "usb_keyboard_report.hpp"
#include <cassert>
#include <cstdio>

int main() {
    UsbKeyboardReport parser;
    std::array<KeyEvent, 6> keys;
    uint8_t report[8] = {0, 0, 40, 0, 0, 0, 0, 0};
    assert(parser.parse(report, 8, keys) == 1 && keys[0].key == Key::Enter);
    assert(parser.parse(report, 8, keys) == 0);  // Held Enter must not send again.
    report[2] = 0;
    assert(parser.parse(report, 8, keys) == 0);
    report[2] = 40;
    assert(parser.parse(report, 8, keys) == 1);  // A real second press is delivered.
    report[2] = 1;
    assert(parser.parse(report, 8, keys) == 0);  // Rollover retains prior state.
    report[2] = 40;
    assert(parser.parse(report, 8, keys) == 0);
    assert(parser.parse(nullptr, 8, keys) == 0);
    assert(parser.parse(report, 7, keys) == 0);
    assert(parser.parse(report, 9, keys) == 0);

    parser.reset();
    report[2] = report[3] = 4;
    assert(parser.parse(report, 8, keys) == 1 && keys[0].character == 'a');
    report[0] = 2;
    assert(parser.parse(report, 8, keys) == 0);  // Modifier changes alone are not new presses.
    report[2] = report[3] = 0;
    parser.parse(report, 8, keys);
    report[2] = 4;
    assert(parser.parse(report, 8, keys) == 1 && keys[0].character == 'A');

    parser.reset();
    report[0] = 0;
    report[2] = 4; report[3] = 57;  // Caps toggles regardless of slot ordering.
    assert(parser.parse(report, 8, keys) == 1 && keys[0].character == 'A');
    report[2] = report[3] = 0;
    parser.parse(report, 8, keys);
    report[0] = 2; report[2] = 5;
    assert(parser.parse(report, 8, keys) == 1 && keys[0].character == 'b');  // Shift XOR Caps.
    report[0] = 1; report[2] = 17;
    assert(parser.parse(report, 8, keys) == 1 && keys[0].control && keys[0].character == 'N');

    // Each connected keyboard gets independent held-key and Caps Lock state.
    UsbKeyboardReport second;
    report[0] = 0; report[2] = 40;
    assert(second.parse(report, 8, keys) == 1);
    parser.reset();  // Disconnect/reconnect must release held keys and reset Caps.
    assert(parser.parse(report, 8, keys) == 1);
    report[2] = 4;
    assert(parser.parse(report, 8, keys) == 1 && keys[0].character == 'a');
    assert(decodeKeyboardKey(0, 88).key == Key::Enter);
    assert(decodeKeyboardKey(0, 89).character == '1');
    assert(decodeKeyboardKey(0, 98).character == '0');
    puts("USB keyboard press/release, rollover, duplicate, modifier and reconnect-state tests passed.");
}
