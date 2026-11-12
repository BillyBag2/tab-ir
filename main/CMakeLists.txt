if(CONFIG_ESP32P4_SELECTS_REV_LESS_V3 AND NOT CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ EQUAL 360)
    message(FATAL_ERROR
        "TAB5 with a pre-v3 ESP32-P4 requires a 360 MHz CPU clock. "
        "Select Component config > ESP System Settings > CPU frequency > 360 MHz "
        "in menuconfig using this build directory's SDKCONFIG, then rebuild.")
endif()

idf_component_register(
    SRCS "main.cpp" "remote_model.cpp" "profile_store.cpp" "ir_transport.cpp" "remote_app.cpp"
         "ir_decoder.cpp" "keyboard_keys.cpp" "tab5_keyboard.cpp"
         "usb_keyboard.cpp" "usb_keyboard_report.cpp"
    INCLUDE_DIRS "."
    REQUIRES M5Unified nvs_flash esp_driver_rmt esp_driver_gpio esp_timer usb_host_hid usb)
