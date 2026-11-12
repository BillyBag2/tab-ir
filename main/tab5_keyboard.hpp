#pragma once
#include "keyboard_keys.hpp"
#include <M5Unified.h>

// TAB5 Keyboard A164, Ext.Port1: SDA0/SCL1, I2C address 0x6D.
// Bounded polling avoids adding an interrupt handler to the display/touch system.
class Tab5Keyboard {
public:
    void begin();
    void update();
    bool poll(KeyEvent& event);
    bool connected() const { return connected_; }
private:
    bool read(uint8_t reg, uint8_t* bytes, size_t count);
    bool write(uint8_t reg, uint8_t value);
    bool busReady_ = false, connected_ = false;
    int64_t nextProbe_ = 0, nextPoll_ = 0;
    unsigned budget_ = 0;
};
