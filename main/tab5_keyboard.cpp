#include "tab5_keyboard.hpp"
#include "esp_timer.h"

namespace { constexpr uint8_t kAddress = 0x6d; constexpr uint32_t kFrequency = 100000; }
bool Tab5Keyboard::read(uint8_t reg, uint8_t* bytes, size_t count) {
    return M5.Ex_I2C.readRegister(kAddress, reg, bytes, count, kFrequency);
}
bool Tab5Keyboard::write(uint8_t reg, uint8_t value) {
    return M5.Ex_I2C.writeRegister8(kAddress, reg, value, kFrequency);
}
void Tab5Keyboard::begin() {
    // Internal touch/power devices use I2C1. Move external I2C0 off Grove:
    // GPIO53/54 remain exclusively owned by the IR RMT channels.
    busReady_ = M5.Ex_I2C.begin(I2C_NUM_0, 0, 1);
    update();
}
void Tab5Keyboard::update() {
    budget_ = 0;
    auto now = esp_timer_get_time();
    if (!busReady_) return;
    if (!connected_) {
        if (now < nextProbe_) return;
        nextProbe_ = now + 2000000;
        uint8_t version = 0;
        // HID mode supplies printable usages, modifiers and navigation keys.
        connected_ = read(0xfe, &version, 1) && write(0x10, 1) &&
                     write(0x00, 0) && write(0x02, 0) && write(0x01, 0);
        if (!connected_) return;
    }
    if (now >= nextPoll_) { budget_ = 8; nextPoll_ = now + 20000; }
}
bool Tab5Keyboard::poll(KeyEvent& event) {
    if (!connected_ || !budget_) return false;
    --budget_;
    uint8_t data[2]{};
    if (!read(0x30, data, sizeof(data))) {
        connected_ = false;
        nextProbe_ = esp_timer_get_time() + 2000000;
        budget_ = 0;
        return false;
    }
    if (data[0] == 0xff && data[1] == 0xff) { budget_ = 0; return false; }
    event = decodeKeyboardKey(data[0], data[1]);
    return true;
}
