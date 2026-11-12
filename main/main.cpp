#include <M5Unified.h>
#include "remote_app.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
    auto config = M5.config();
    config.external_imu = false;
    config.external_rtc = false;
    config.external_display_value = 0;  // Do not probe I2C accessories on the Grove IR pins.
    config.internal_spk = false;
    config.internal_mic = false;
    M5.begin(config);
    M5.Display.setRotation(3);  // Opposite landscape orientation (180 degrees from rotation 1).
    M5.Display.setBrightness(160);
    M5.Power.setExtOutput(true);  // Supply the Grove IR unit's external 5V rail.
    static RemoteApp app;
    app.begin();
    while (true) {
        M5.update();
        app.update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
