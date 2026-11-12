#pragma once
#include "remote_model.hpp"

namespace remote {
enum class Protocol { Unknown, Nec, NecExtended, Samsung32, Sony };
struct DecodedMessage {
    Protocol protocol = Protocol::Unknown;
    uint32_t raw = 0;
    uint16_t address = 0;
    uint8_t command = 0, bits = 0;
};
// RMT measures the waveform in hardware; protocol interpretation runs on the CPU.
DecodedMessage decodeIr(const Message& message);
std::string describeIr(const Message& message);
}
