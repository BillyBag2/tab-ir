#include "ir_decoder.hpp"
#include <cstdio>

namespace remote {
namespace {
bool near(uint16_t actual, unsigned expected) {
    // Allow receiver mark/space distortion and ordinary remote oscillator error.
    return actual >= expected * 75 / 100 && actual <= expected * 125 / 100;
}
bool distanceBits(const Message& message, uint32_t& raw) {
    if (message.pulses.size() != 67 || !near(message.pulses.back(), 560)) return false;
    raw = 0;
    for (unsigned i = 0; i < 32; ++i) {
        if (!near(message.pulses[2 + 2 * i], 560)) return false;
        auto space = message.pulses[3 + 2 * i];
        if (near(space, 1690)) raw |= uint32_t(1) << i;
        else if (!near(space, 560)) return false;
    }
    return true;
}
}
DecodedMessage decodeIr(const Message& message) {
    DecodedMessage result;
    if (!validMessage(message)) return result;
    const auto& p = message.pulses;
    uint32_t raw = 0;
    if (near(p[1], 4500) && (near(p[0], 9000) || near(p[0], 4500)) && distanceBits(message, raw)) {
        uint8_t address = raw, second = raw >> 8, command = raw >> 16, inverse = raw >> 24;
        // Check command redundancy before claiming a known protocol. NEC-like
        // protocols without this check remain raw, rather than being mislabelled.
        if (uint8_t(command ^ inverse) != 0xff) return result;
        result.raw = raw;
        result.bits = 32;
        result.command = command;
        if (near(p[0], 9000)) {
            bool standard = uint8_t(address ^ second) == 0xff;
            result.protocol = standard ? Protocol::Nec : Protocol::NecExtended;
            result.address = standard ? address : uint16_t(raw);
        } else {
            result.protocol = Protocol::Samsung32;
            result.address = uint16_t(raw);
        }
        return result;
    }
    // Sony SIRC is pulse-width encoded, has no stop mark, and sends 12/15/20 bits LSB first.
    if (p.size() % 2 != 1 || !near(p[0], 2400) || !near(p[1], 600)) return {};
    size_t bits = (p.size() - 1) / 2;
    if (bits != 12 && bits != 15 && bits != 20) return {};
    for (size_t i = 0; i < bits; ++i) {
        auto mark = p[2 + i * 2];
        if (near(mark, 1200)) raw |= uint32_t(1) << i;
        else if (!near(mark, 600)) return {};
        if (i + 1 < bits && !near(p[3 + i * 2], 600)) return {};
    }
    result.protocol = Protocol::Sony;
    result.raw = raw;
    result.bits = bits;
    result.command = raw & 0x7f;
    result.address = raw >> 7;
    return result;
}
std::string describeIr(const Message& message) {
    if (message.empty()) return "No message";
    auto decoded = decodeIr(message);
    if (decoded.protocol == Protocol::Unknown) return "Unknown protocol / raw timings retained";
    const char* protocol = "Sony SIRC";
    if (decoded.protocol == Protocol::Nec) protocol = "NEC";
    if (decoded.protocol == Protocol::NecExtended) protocol = "NEC extended";
    if (decoded.protocol == Protocol::Samsung32) protocol = "Samsung32";
    char text[112];
    snprintf(text, sizeof(text), "%s %u-bit / Address 0x%04X / Command 0x%02X / Data 0x%08lX",
             protocol, unsigned(decoded.bits), unsigned(decoded.address), unsigned(decoded.command),
             static_cast<unsigned long>(decoded.raw));
    return text;
}
}
