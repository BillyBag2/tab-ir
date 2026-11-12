#pragma once
#include "ir_transport.hpp"
#include "profile_store.hpp"
#include "tab5_keyboard.hpp"
#include "usb_keyboard.hpp"

// Application state and touch UI. Hardware and persistence live in separate modules.
class RemoteApp {
public:
    void begin();
    void update();
private:
    struct Hit { int x, y, w, h, action; };
    enum class Screen { Main, Name, ConfirmDelete, ConfirmClear };
    void draw();
    void button(int x, int y, int w, int h, const std::string& text, int action, bool selected = false);
    void label(int x, int y, const std::string& text, int size = 2, uint32_t color = 0xffffff);
    void action(int id);
    void key(const KeyEvent& event);
    void report(esp_err_t err, const char* success);
    bool persist(const remote::Profile& replacement);
    ProfileStore store_;
    IrTransport ir_;
    Tab5Keyboard keyboard_;
    UsbKeyboard usbKeyboard_;
    std::array<remote::Profile, remote::kMaxProfiles> profiles_;
    std::array<bool, remote::kMaxProfiles> occupied_{};
    std::vector<Hit> hits_;
    remote::Message captured_;
    size_t slot_ = 0, button_ = 0;
    Screen screen_ = Screen::Main;
    bool ready_ = false, learning_ = false, rename_ = false;
    int64_t deadline_ = 0;
    std::string name_, status_;
};
