#pragma once
#include "usb_keyboard_report.hpp"
#include "usb/hid_host.h"
#include "freertos/queue.h"
#include <atomic>

// Owns the USB-A host and queues application keys to the UI task. Display
// and I2C power calls never run inside USB callbacks.
class UsbKeyboard {
public:
    esp_err_t begin();
    bool poll(KeyEvent& event);
    unsigned connected() const { return connected_.load(); }
    esp_err_t error() const { return error_.load(); }
private:
    struct Interface {
        UsbKeyboard* owner = nullptr;
        hid_host_device_handle_t handle = nullptr;
        UsbKeyboardReport parser;
        bool active = false;
    };
    struct ControlEvent { hid_host_device_handle_t handle; bool connect; };
    static void hostTask(void* context);
    static void hidTask(void* context);
    static void deviceEvent(hid_host_device_handle_t, hid_host_driver_event_t, void*);
    static void interfaceEvent(hid_host_device_handle_t, hid_host_interface_event_t, void*);
    void open(hid_host_device_handle_t handle);
    void close(hid_host_device_handle_t handle);
    void defer(hid_host_device_handle_t handle, bool connect);
    std::array<Interface, 4> interfaces_{};
    QueueHandle_t controls_ = nullptr, keys_ = nullptr;
    std::atomic<unsigned> connected_{0};
    std::atomic<esp_err_t> error_{ESP_OK};
    std::atomic<bool> stopHost_{false};
};
