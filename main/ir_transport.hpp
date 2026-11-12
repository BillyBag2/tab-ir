#pragma once
#include "remote_model.hpp"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Public operations run in the UI task. The receive ISR only queues completion.
class IrTransport {
public:
    esp_err_t begin();
    esp_err_t startLearning();
    esp_err_t cancelLearning();
    bool poll(remote::Message& message, esp_err_t& result);
    esp_err_t send(const remote::Message& message);
private:
    static bool received(rmt_channel_handle_t, const rmt_rx_done_event_data_t*, void*);
    rmt_channel_handle_t rx_ = nullptr, tx_ = nullptr;
    rmt_encoder_handle_t encoder_ = nullptr;
    QueueHandle_t completed_ = nullptr;
    // IDF DMA may align its buffer pointer and size to a cache line. Align both
    // here so completion data starts at buffer_[0] and truncation checks are exact.
    alignas(64) rmt_symbol_word_t buffer_[528] = {};
};
