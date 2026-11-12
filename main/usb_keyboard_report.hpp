#pragma once
#include "keyboard_keys.hpp"
#include <array>
#include <cstddef>

// One parser per keyboard interface. USB boot reports contain a modifier byte,
// a reserved byte and six held-key usages, rather than individual key presses.
class UsbKeyboardReport {
public:
    size_t parse(const uint8_t* report, size_t length, std::array<KeyEvent, 6>& pressed);
    void reset() { previous_.fill(0); capsLock_ = false; }
private:
    std::array<uint8_t, 6> previous_{};
    bool capsLock_ = false;
};
