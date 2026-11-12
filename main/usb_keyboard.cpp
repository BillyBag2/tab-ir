#include "usb_keyboard.hpp"
#include <M5Unified.h>
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/task.h"

namespace { constexpr const char* kTag = "usb_keyboard"; }
esp_err_t UsbKeyboard::begin() {
    // TAB5 USB-A VBUS is IO expander 1, pin 3. M5Unified routes ext_USB
    // correctly; setUsbOutput() is not the TAB5 USB-A power API.
    if (!M5.Power.setExtOutput(true, m5::ext_USB)) return error_ = ESP_FAIL;
    controls_ = xQueueCreate(32, sizeof(ControlEvent));
    keys_ = xQueueCreate(64, sizeof(KeyEvent));
    if (!controls_ || !keys_) {
        if (controls_) vQueueDelete(controls_);
        if (keys_) vQueueDelete(keys_);
        controls_ = keys_ = nullptr;
        return error_ = ESP_ERR_NO_MEM;
    }
    usb_host_config_t host{};
    host.intr_flags = ESP_INTR_FLAG_LOWMED;
    // On P4 the default is the high-speed controller wired to USB-A. Leave
    // the USB-C serial/JTAG programming port outside the host configuration.
    auto err = usb_host_install(&host);
    if (err != ESP_OK) return error_ = err;
    hid_host_driver_config_t hid{};
    // Class control requests wait for completions dispatched by this separate
    // driver task. They must never block the same task that dispatches HID events.
    hid.create_background_task = true;
    hid.task_priority = 5;
    hid.stack_size = 4096;
    hid.core_id = 0;
    hid.callback = deviceEvent;
    hid.callback_arg = this;
    err = hid_host_install(&hid);
    if (err != ESP_OK) { usb_host_uninstall(); return error_ = err; }
    if (xTaskCreate(hostTask, "usb_host", 4096, this, 2, nullptr) != pdPASS) {
        hid_host_uninstall();
        usb_host_uninstall();
        return error_ = ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(hidTask, "usb_keyboard", 6144, this, 3, nullptr) != pdPASS) {
        hid_host_uninstall();
        stopHost_ = true;  // Host task releases its own peripheral after dispatch stops.
        return error_ = ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
void UsbKeyboard::hostTask(void* context) {
    auto* self = static_cast<UsbKeyboard*>(context);
    while (!self->stopHost_) {
        uint32_t flags = 0;
        auto err = usb_host_lib_handle_events(pdMS_TO_TICKS(100), &flags);
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) self->error_ = err;
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
    }
    usb_host_device_free_all();
    usb_host_uninstall();
    vTaskDelete(nullptr);
}
void UsbKeyboard::hidTask(void* context) {
    auto* self = static_cast<UsbKeyboard*>(context);
    while (true) {
        // Blocking open/close/control requests run outside the HID callbacks,
        // since callbacks execute while dispatching client USB events.
        ControlEvent event;
        if (xQueueReceive(self->controls_, &event, portMAX_DELAY) == pdTRUE) {
            if (event.connect) self->open(event.handle);
            else self->close(event.handle);
        }
    }
}
void UsbKeyboard::defer(hid_host_device_handle_t handle, bool connect) {
    ControlEvent event{handle, connect};
    if (xQueueSend(controls_, &event, 0) != pdTRUE) {
        error_ = ESP_ERR_NO_MEM;
        ESP_LOGE(kTag, "USB control event queue full");
    }
}
void UsbKeyboard::deviceEvent(hid_host_device_handle_t handle, hid_host_driver_event_t event, void* context) {
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) static_cast<UsbKeyboard*>(context)->defer(handle, true);
}
void UsbKeyboard::open(hid_host_device_handle_t handle) {
    hid_host_dev_params_t params{};
    if (hid_host_device_get_params(handle, &params) != ESP_OK) return;
    if (params.sub_class != HID_SUBCLASS_BOOT_INTERFACE || params.proto != HID_PROTOCOL_KEYBOARD) {
        ESP_LOGI(kTag, "Ignoring non-boot-keyboard HID interface %u", unsigned(params.iface_num));
        return;
    }
    for (auto& iface : interfaces_) if (iface.handle == handle) return;
    for (auto& iface : interfaces_) {
        if (iface.handle) continue;
        iface.owner = this;
        iface.handle = handle;
        iface.parser.reset();
        hid_host_device_config_t config{};
        config.callback = interfaceEvent;
        config.callback_arg = &iface;
        auto err = hid_host_device_open(handle, &config);
        bool opened = err == ESP_OK;
        if (opened) err = hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT);
        if (err == ESP_OK) err = hid_class_request_set_idle(handle, 0, 0);
        if (err == ESP_OK) err = hid_host_device_start(handle);
        if (err != ESP_OK) {
            if (!opened || hid_host_device_close(handle) == ESP_OK) iface.handle = nullptr;
            error_ = err;
            ESP_LOGW(kTag, "Keyboard interface could not start: %s", esp_err_to_name(err));
            return;
        }
        iface.active = true;
        ++connected_;
        error_ = ESP_OK;
        ESP_LOGI(kTag, "USB keyboard connected");
        return;
    }
    ESP_LOGW(kTag, "Maximum of four keyboard interfaces reached");
}
void UsbKeyboard::close(hid_host_device_handle_t handle) {
    for (auto& iface : interfaces_) {
        if (iface.handle != handle) continue;
        if (iface.active) --connected_;
        iface.active = false;
        auto err = hid_host_device_close(handle);
        if (err != ESP_OK) {
            error_ = err;
            ESP_LOGW(kTag, "Keyboard interface close failed: %s", esp_err_to_name(err));
            return;  // Keep callback context alive if the interface is still open.
        }
        iface.parser.reset();
        iface.handle = nullptr;
        return;
    }
}
void UsbKeyboard::interfaceEvent(hid_host_device_handle_t handle, hid_host_interface_event_t event, void* context) {
    auto* iface = static_cast<Interface*>(context);
    auto* self = iface->owner;
    if (event == HID_HOST_INTERFACE_EVENT_INPUT_REPORT) {
        uint8_t report[64];
        size_t length = 0;
        if (hid_host_device_get_raw_input_report_data(handle, report, sizeof(report), &length) != ESP_OK) return;
        std::array<KeyEvent, 6> pressed;
        size_t count = iface->parser.parse(report, length, pressed);
        for (size_t i = 0; i < count; ++i) {
            if (xQueueSend(self->keys_, &pressed[i], 0) != pdTRUE) {
                ESP_LOGW(kTag, "Keyboard input queue full; key dropped");
            }
        }
    } else if (event == HID_HOST_INTERFACE_EVENT_DISCONNECTED || event == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) {
        if (event == HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR) self->error_ = ESP_FAIL;
        self->defer(handle, false);
    }
}
bool UsbKeyboard::poll(KeyEvent& event) {
    return keys_ && xQueueReceive(keys_, &event, 0) == pdTRUE;
}
