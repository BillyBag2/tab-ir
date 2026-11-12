#include "remote_app.hpp"
#include <M5Unified.h>
#include "esp_timer.h"
#include "ir_decoder.hpp"
#include "esp_log.h"
#include <algorithm>

namespace {
constexpr uint32_t kBackground = 0x101c2c, kPanel = 0x25384c, kAccent = 0x007f85;
constexpr const char* kKeys = "1234567890QWERTYUIOPASDFGHJKL-ZXCVBNM_./";
}
void RemoteApp::begin() {
    keyboard_.begin();
    auto usbResult = usbKeyboard_.begin();
    if (usbResult != ESP_OK) ESP_LOGW("app", "USB keyboard unavailable: %s", esp_err_to_name(usbResult));
    auto err = store_.begin();
    if (err != ESP_OK) { report(err, ""); draw(); return; }
    bool damaged = false;
    for (size_t i = 0; i < profiles_.size(); ++i) {
        err = store_.load(i, profiles_[i]);
        occupied_[i] = err == ESP_OK;
        // Do not overwrite a slot that exists but cannot be read.
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            status_ = "Unreadable saved profile: " + std::string(esp_err_to_name(err));
            damaged = true;
        }
    }
    if (damaged) { draw(); return; }
    for (size_t i = 0; i < occupied_.size(); ++i) if (occupied_[i]) { slot_ = i; break; }
    err = ir_.begin();
    if (err != ESP_OK) { report(err, ""); draw(); return; }
    ready_ = true;
    status_ = "Select a remote and button type, then Learn or Send.";
    draw();
}
void RemoteApp::report(esp_err_t err, const char* success) {
    status_ = err == ESP_OK ? success : std::string("Error: ") + esp_err_to_name(err);
}
bool RemoteApp::persist(const remote::Profile& replacement) {
    auto err = store_.save(slot_, replacement);
    report(err, "Saved to flash.");
    if (err != ESP_OK) return false;
    profiles_[slot_] = replacement;
    occupied_[slot_] = true;
    return true;
}
void RemoteApp::label(int x, int y, const std::string& text, int size, uint32_t color) {
    auto& display = M5.Display;
    display.setTextSize(size);
    display.setTextColor(color);
    display.setCursor(x * display.width() / 1280, y * display.height() / 720);
    display.print(text.c_str());
}
void RemoteApp::button(int x, int y, int w, int h, const std::string& text, int id, bool selected) {
    auto& display = M5.Display;
    display.fillRoundRect(x * display.width() / 1280, y * display.height() / 720,
                          w * display.width() / 1280, h * display.height() / 720,
                          9, selected ? kAccent : kPanel);
    label(x + 14, y + h / 2 - 8, text);
    hits_.push_back({x, y, w, h, id});
}
void RemoteApp::draw() {
    M5.Display.fillScreen(kBackground);
    hits_.clear();
    label(24, 20, "IR REMOTE STUDIO", 3);
    std::string keyboardStatus = "TAB5 / Grove IR / TAB5 keyboard: ";
    keyboardStatus += keyboard_.connected() ? "connected" : "absent";
    keyboardStatus += " / USB: ";
    keyboardStatus += usbKeyboard_.connected() ? "connected" : usbKeyboard_.error() != ESP_OK ? "error (see serial log)" : "ready";
    label(24, 62, keyboardStatus, 2, 0x8fb9ce);
    if (!ready_) { label(24, 140, status_); return; }
    if (screen_ == Screen::Name) {
        label(24, 115, rename_ ? "Rename remote profile" : "New remote profile (part number or name)");
        label(24, 162, name_ + "_", 3);
        label(24, 210, "Physical keyboard: type name, Enter saves, Esc cancels.", 2, 0x8fb9ce);
        for (int i = 0; i < 40; ++i)
            button(24 + (i % 10) * 124, 240 + (i / 10) * 76, 114, 66, std::string(1, kKeys[i]), 200 + i);
        button(24, 565, 220, 65, "Space", 31);
        button(260, 565, 220, 65, "Backspace", 32);
        button(496, 565, 220, 65, "Cancel", 33);
        button(732, 565, 220, 65, "Save name", 34, true);
    } else if (screen_ == Screen::ConfirmDelete || screen_ == Screen::ConfirmClear) {
        label(24, 160, screen_ == Screen::ConfirmDelete ? "Delete this remote and all its learned buttons?" : "Clear the saved message for this button?");
        label(24, 210, profiles_[slot_].name + " / " + remote::kButtons[button_]);
        button(24, 300, 280, 80, "Cancel", 33);
        button(328, 300, 280, 80, "Confirm", 35, true);
    } else {
        button(24, 105, 95, 62, "<", 1);
        label(140, 126, occupied_[slot_] ? profiles_[slot_].name : "No remote - tap New", 3);
        button(720, 105, 95, 62, ">", 2);
        button(835, 105, 125, 62, "New", 3);
        button(975, 105, 125, 62, "Rename", 4);
        button(1115, 105, 140, 62, "Delete", 5);
        for (size_t i = 0; i < remote::kButtons.size(); ++i) {
            std::string caption = remote::kButtons[i];
            if (occupied_[slot_] && !profiles_[slot_].messages[i].empty()) caption += " *";
            button(24 + (i % 6) * 207, 190 + (i / 6) * 78, 195, 66, caption, 100 + i, i == button_);
        }
        std::string detail = std::string("Selected: ") + remote::kButtons[button_];
        const auto& saved = profiles_[slot_].messages[button_];
        detail += saved.empty() ? " / no saved message" : " / " + std::to_string(saved.pulses.size()) + " timings saved";
        label(24, 516, detail);
        // Decode on selection as well as capture, so existing saved profiles gain
        // readable messages without changing their on-flash representation.
        label(24, 540, remote::describeIr(captured_.empty() ? saved : captured_), 2, 0x8fb9ce);
        if (learning_) {
            button(24, 560, 260, 70, "Cancel learning", 14, true);
        } else if (!captured_.empty()) {
            button(24, 560, 260, 70, "Save assignment", 11, true);
            button(300, 560, 210, 70, "Test capture", 12);
            button(526, 560, 210, 70, "Discard", 13);
        } else {
            button(24, 560, 210, 70, "Learn", 6, true);
            button(250, 560, 210, 70, "Send", 7);
            button(476, 560, 210, 70, "Clear button", 8);
        }
        if (learning_) label(24, 643, "Keyboard: Esc cancels learning.", 2, 0x8fb9ce);
        else if (!captured_.empty()) label(24, 643, "Keyboard: Enter saves | T tests capture | Esc discards", 2, 0x8fb9ce);
        else {
            label(24, 643, "Keys: arrows select | Ctrl+Left/Right remote | Enter sends | * = saved", 2, 0x8fb9ce);
            label(24, 665, "Ctrl+N new | Ctrl+R rename | Ctrl+L learn | Ctrl+D delete | Del clear", 2, 0x8fb9ce);
        }
    }
    label(24, 684, status_, 2, 0xffd082);
}
void RemoteApp::action(int id) {
    // Lock the assignment target while a capture is pending or being received.
    if (screen_ == Screen::Main && (learning_ || !captured_.empty()) && id != 11 && id != 12 && id != 13 && id != 14) return;
    if (id >= 4 && id <= 8 && !occupied_[slot_]) { status_ = "Create a named remote with New first."; return; }
    if (id >= 100 && id < 124) { button_ = id - 100; status_ = "Button selected."; }
    else if (id == 1 || id == 2) {
        for (size_t n = 1; n <= occupied_.size(); ++n) {
            size_t candidate = (slot_ + (id == 2 ? n : occupied_.size() - n)) % occupied_.size();
            if (occupied_[candidate]) { slot_ = candidate; break; }
        }
    } else if (id == 3) {
        auto free = std::find(occupied_.begin(), occupied_.end(), false);
        if (free == occupied_.end()) status_ = "Eight profiles already stored. Delete one to add another.";
        else { rename_ = false; name_.clear(); screen_ = Screen::Name; status_ = "Name: 1-32 characters."; }
    } else if (id == 4 && occupied_[slot_]) {
        rename_ = true; name_ = profiles_[slot_].name; screen_ = Screen::Name;
    } else if (id == 5 && occupied_[slot_]) screen_ = Screen::ConfirmDelete;
    else if (id == 6 && occupied_[slot_]) {
        auto err = ir_.startLearning();
        learning_ = err == ESP_OK;
        deadline_ = esp_timer_get_time() + 10000000;
        report(err, "Point remote at receiver and briefly press the button (10 seconds).");
    } else if (id == 7 && occupied_[slot_]) {
        if (profiles_[slot_].messages[button_].empty()) status_ = "No saved message for this button. Use Learn first.";
        else report(ir_.send(profiles_[slot_].messages[button_]), "Saved message sent.");
    }
    else if (id == 8 && occupied_[slot_]) screen_ = Screen::ConfirmClear;
    else if (id == 11) {
        auto replacement = profiles_[slot_];
        replacement.messages[button_] = captured_;
        if (persist(replacement)) captured_.pulses.clear();
    } else if (id == 12) report(ir_.send(captured_), "Captured message sent. Save to keep it.");
    else if (id == 13) { captured_.pulses.clear(); status_ = "Capture discarded; stored message preserved."; }
    else if (id == 14) { report(ir_.cancelLearning(), "Learning cancelled."); learning_ = false; }
    else if (id >= 200 && id < 240 && name_.size() < 32) name_ += kKeys[id - 200];
    else if (id == 31 && name_.size() < 32) name_ += ' ';
    else if (id == 32 && !name_.empty()) name_.pop_back();
    else if (id == 33) { screen_ = Screen::Main; status_ = "Cancelled."; }
    else if (id == 34) {
        auto first = name_.find_first_not_of(' '), last = name_.find_last_not_of(' ');
        if (first == std::string::npos) { status_ = "Enter a name first."; return; }
        std::string cleaned = name_.substr(first, last - first + 1);
        for (size_t i = 0; i < profiles_.size(); ++i) {
            if (occupied_[i] && (!rename_ || i != slot_) && profiles_[i].name == cleaned) {
                status_ = "That name already exists."; return;
            }
        }
        size_t previous = slot_;
        if (!rename_) slot_ = std::find(occupied_.begin(), occupied_.end(), false) - occupied_.begin();
        auto replacement = rename_ ? profiles_[slot_] : remote::Profile{};
        replacement.name = cleaned;
        if (persist(replacement)) screen_ = Screen::Main;
        else slot_ = previous;
    } else if (id == 35) {
        if (screen_ == Screen::ConfirmDelete) {
            auto err = store_.remove(slot_);
            report(err, "Remote deleted.");
            if (err == ESP_OK) { occupied_[slot_] = false; profiles_[slot_] = {}; action(2); }
        } else {
            auto replacement = profiles_[slot_];
            replacement.messages[button_] = {};
            persist(replacement);
        }
        screen_ = Screen::Main;
    }
}
void RemoteApp::key(const KeyEvent& event) {
    if (event.alt || event.key == Key::None) return;
    if (screen_ == Screen::Name) {
        if (event.key == Key::Escape) action(33);
        else if (event.key == Key::Enter) action(34);
        else if (event.key == Key::Backspace) action(32);
        else if (event.key == Key::Character && !event.control && name_.size() < 32 &&
                 event.character >= 32 && event.character <= 126) name_ += event.character;
        return;
    }
    if (screen_ == Screen::ConfirmDelete || screen_ == Screen::ConfirmClear) {
        if (event.key == Key::Escape) action(33);
        else if (event.key == Key::Enter) action(35);
        return;
    }
    if (learning_) { if (event.key == Key::Escape) action(14); return; }
    if (!captured_.empty()) {
        if (event.key == Key::Escape) action(13);
        else if (event.key == Key::Enter || (event.control && (event.character == 's' || event.character == 'S'))) action(11);
        else if (event.character == 't' || event.character == 'T') action(12);
        return;
    }
    if (event.control && event.key == Key::Left) action(1);
    else if (event.control && event.key == Key::Right) action(2);
    else if (event.key == Key::PageUp) action(1);
    else if (event.key == Key::PageDown) action(2);
    else if (event.key == Key::Left) action(100 + (button_ + 23) % 24);
    else if (event.key == Key::Right || event.key == Key::Tab) action(100 + (button_ + 1) % 24);
    else if (event.key == Key::Up) action(100 + (button_ + 18) % 24);
    else if (event.key == Key::Down) action(100 + (button_ + 6) % 24);
    else if (event.key == Key::Enter) action(7);
    else if (event.key == Key::Delete) action(8);
    else if (event.control && event.key == Key::Character) {
        switch (event.character | 0x20) {
            case 'n': action(3); break;
            case 'r': action(4); break;
            case 'd': action(5); break;
            case 'l': action(6); break;
            case 's': action(7); break;
            default: break;
        }
    }
}
void RemoteApp::update() {
    if (!ready_) return;
    bool wasConnected = keyboard_.connected();
    auto usbCount = usbKeyboard_.connected();
    auto usbError = usbKeyboard_.error();
    keyboard_.update();
    bool keyboardDirty = false;
    KeyEvent event;
    while (keyboard_.poll(event)) {
        if (event.key != Key::None) { key(event); keyboardDirty = true; }
    }
    for (unsigned i = 0; i < 8 && usbKeyboard_.poll(event); ++i) {
        key(event);
        keyboardDirty = true;
    }
    // USB callbacks can run between UI updates. Remember what was displayed,
    // rather than only comparing two snapshots taken within this update.
    static unsigned shownUsbCount = 0;
    static esp_err_t shownUsbError = ESP_OK;
    if (keyboardDirty || wasConnected != keyboard_.connected() ||
        shownUsbCount != usbCount || shownUsbError != usbError) {
        draw();
        shownUsbCount = usbCount;
        shownUsbError = usbError;
    }
    if (learning_) {
        esp_err_t result;
        if (ir_.poll(captured_, result)) {
            learning_ = false;
            if (result != ESP_OK) captured_.pulses.clear();
            report(result, "Captured. Test it, then Save assignment to keep it.");
            draw();
        } else if (esp_timer_get_time() >= deadline_) {
            learning_ = false;
            report(ir_.cancelLearning(), "No message received. Try Learn again.");
            draw();
        }
    }
    auto touch = M5.Touch.getDetail();
    if (touch.wasPressed()) {
        int x = touch.x * 1280 / M5.Display.width(), y = touch.y * 720 / M5.Display.height();
        for (const auto& hit : hits_) {
            if (x >= hit.x && x < hit.x + hit.w && y >= hit.y && y < hit.y + hit.h) {
                int id = hit.action;
                action(id);
                draw();
                break;
            }
        }
    }
}
