#include "ir_transport.hpp"
#include "driver/gpio.h"
#include "ir_decoder.hpp"

bool IrTransport::received(rmt_channel_handle_t, const rmt_rx_done_event_data_t* event, void* context) {
    auto* self = static_cast<IrTransport*>(context);
    BaseType_t wake = pdFALSE;
    size_t count = event->num_symbols;
    xQueueSendFromISR(self->completed_, &count, &wake);
    return wake == pdTRUE;
}
esp_err_t IrTransport::begin() {
    completed_ = xQueueCreate(1, sizeof(size_t));
    if (!completed_) return ESP_ERR_NO_MEM;
    // TAB5 Grove yellow = GPIO53 (TX), white = GPIO54 (active-low RX).
    rmt_rx_channel_config_t rx{};
    rx.gpio_num = GPIO_NUM_54;
    rx.clk_src = RMT_CLK_SRC_DEFAULT;
    rx.resolution_hz = 1000000;
    rx.mem_block_symbols = 512;
    rx.flags.with_dma = true;
    auto err = rmt_new_rx_channel(&rx, &rx_);
    if (err != ESP_OK) return err;
    gpio_pullup_en(GPIO_NUM_54);
    rmt_rx_event_callbacks_t callbacks{};
    callbacks.on_recv_done = received;
    if ((err = rmt_rx_register_event_callbacks(rx_, &callbacks, this)) != ESP_OK) return err;
    if ((err = rmt_enable(rx_)) != ESP_OK) return err;
    rmt_tx_channel_config_t tx{};
    tx.gpio_num = GPIO_NUM_53;
    tx.clk_src = RMT_CLK_SRC_DEFAULT;
    tx.resolution_hz = 1000000;
    tx.mem_block_symbols = 64;
    tx.trans_queue_depth = 1;
    if ((err = rmt_new_tx_channel(&tx, &tx_)) != ESP_OK) return err;
    rmt_carrier_config_t carrier{};
    carrier.frequency_hz = 38000;
    carrier.duty_cycle = 0.33f;
    if ((err = rmt_apply_carrier(tx_, &carrier)) != ESP_OK) return err;
    rmt_copy_encoder_config_t copy{};
    if ((err = rmt_new_copy_encoder(&copy, &encoder_)) != ESP_OK) return err;
    return rmt_enable(tx_);
}
esp_err_t IrTransport::startLearning() {
    xQueueReset(completed_);
    rmt_receive_config_t config{};
    // The hardware glitch filter counts source-clock ticks (8-bit maximum),
    // not 1 us channel ticks. A 1 us threshold fits the P4 source clock.
    config.signal_range_min_ns = 1000;
    config.signal_range_max_ns = 12000000;  // A 12 ms idle ends one frame.
    return rmt_receive(rx_, buffer_, sizeof(buffer_), &config);
}
esp_err_t IrTransport::cancelLearning() {
    auto err = rmt_disable(rx_);  // Stop DMA before reusing the buffer/queue.
    if (err != ESP_OK) return err;
    xQueueReset(completed_);
    return rmt_enable(rx_);
}
bool IrTransport::poll(remote::Message& message, esp_err_t& result) {
    size_t count;
    if (xQueueReceive(completed_, &count, 0) != pdTRUE) return false;
    message.pulses.clear();
    // Full captures may be truncated; never silently save them.
    if (count >= sizeof(buffer_) / sizeof(buffer_[0])) { result = ESP_ERR_INVALID_SIZE; return true; }
    bool started = false;
    auto append = [&](uint16_t duration, bool level) {
        if (!duration || (!started && level)) return;
        started = true;
        bool mark = !level;
        if (mark == ((message.pulses.size() % 2) == 0)) message.pulses.push_back(duration);
        else if (!message.pulses.empty()) {
            unsigned sum = message.pulses.back() + duration;
            message.pulses.back() = sum > 32767 ? 0 : sum;
        }
    };
    for (size_t i = 0; i < count; ++i) {
        append(buffer_[i].duration0, buffer_[i].level0);
        append(buffer_[i].duration1, buffer_[i].level1);
    }
    // Trailing idle is framing, rather than part of the remote's message.
    if (message.pulses.size() % 2 == 0 && !message.empty()) message.pulses.pop_back();
    result = remote::validMessage(message) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
    return true;
}
esp_err_t IrTransport::send(const remote::Message& message) {
    if (!remote::validMessage(message)) return ESP_ERR_INVALID_ARG;
    // RMT generates the carrier in hardware. Sony uses 40 kHz; unknown
    // protocols keep the Unit IR receiver's nominal 38 kHz playback default.
    auto decoded = remote::decodeIr(message);
    rmt_carrier_config_t carrier{};
    carrier.frequency_hz = decoded.protocol == remote::Protocol::Sony ? 40000 : 38000;
    carrier.duty_cycle = 0.33f;
    auto carrierResult = rmt_apply_carrier(tx_, &carrier);
    if (carrierResult != ESP_OK) return carrierResult;
    std::vector<rmt_symbol_word_t> symbols;
    for (size_t i = 0; i < message.pulses.size(); i += 2) {
        rmt_symbol_word_t symbol{};
        symbol.level0 = 1;
        symbol.duration0 = message.pulses[i];
        symbol.level1 = 0;
        symbol.duration1 = i + 1 < message.pulses.size() ? message.pulses[i + 1] : 1;
        symbols.push_back(symbol);
    }
    rmt_transmit_config_t config{};
    auto err = rmt_transmit(tx_, encoder_, symbols.data(), symbols.size() * sizeof(symbols[0]), &config);
    // Keep symbols alive until the driver has consumed the entire frame.
    if (err == ESP_OK) err = rmt_tx_wait_all_done(tx_, -1);
    return err;
}
